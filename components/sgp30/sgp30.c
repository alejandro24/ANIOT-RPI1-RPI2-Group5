#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_event_base.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "portmacro.h"
#include <reent.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "sgp30.h"

ESP_EVENT_DEFINE_BASE(SENSOR_EVENTS);

#define SGP30_MEASURING_INTERVAL 1000000

static char* TAG = "SGP30";
static esp_event_loop_handle_t sgp30_event_loop;
static i2c_master_dev_handle_t sgp30_dev_handle;
static bool g_baseline_is_set = false;
static bool sgp30_is_initialized = false;
static esp_timer_handle_t g_h_baseline_12h_timer;
static esp_timer_handle_t g_h_baseline_1h_timer;
static esp_timer_handle_t g_h_initialization_timer;
static esp_timer_handle_t g_h_measurement_timer;
static SemaphoreHandle_t device_in_use_mutex;
static uint16_t id[3];

static sgp30_measurement_t baseline;

static esp_err_t crc8_check(uint8_t *buffer, size_t size) {
    uint8_t crc = SGP30_CRC_8_INIT;

    for (int i = 0; i < size; i++) {
        // We XOR the first/next bit into the crc
        crc ^= buffer[i];
        for (int j = 0; j < 8; j++) {
            // Check if the MSB is set to 1
            if (crc & 0x80) {
                // If it is set to 1 get rid of the 1 and XOR
                crc = (crc << 1) ^ SGP30_CRC_8_POLY;
            } else {
                // If it is not, just keep looking for a 1
                crc <<= 1;
            }
        }
    }
    // Error if reminder is not zero.
    if (crc) {
        return ESP_ERR_INVALID_CRC;
    } else {
        return ESP_OK;
    }
}

static uint8_t crc8_gen(uint8_t *buffer, size_t size) {
    uint8_t crc = SGP30_CRC_8_INIT;

    for (int i = 0; i < size; i++) {
        crc ^= buffer[i];
        for (int j = 0; j < 8; j++) {
            if ((crc & 0x80) != 0) {
                crc = (crc << 1) ^ SGP30_CRC_8_POLY;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static esp_err_t sgp30_command_w(
    i2c_master_dev_handle_t dev_handle,
    sgp30_register_rw_t command,
    uint8_t write_delay,
    uint16_t *msg,
    size_t msg_len)
{
    size_t msg_buffer_len = 2 + 3 * msg_len;
    uint8_t msg_buffer[2 + msg_buffer_len];
    msg_buffer[0] = (command >> 8) & 0xFF;
    msg_buffer[1] = command  & 0xFF;

    for (int i = 0; i < msg_len; i++) {
        msg_buffer[3 * i + 2] = (msg[i] >> 8) & 0xFF;
        msg_buffer[3 * i + 3] = msg[i] & 0xFF;
        msg_buffer[3 * i + 4] = crc8_gen(msg_buffer + 2 + 3 * i, 2);
    }

    xSemaphoreTake(device_in_use_mutex, portMAX_DELAY);

    esp_err_t got_sent = i2c_master_transmit(dev_handle, msg_buffer, msg_buffer_len, -1);
    if (got_sent != ESP_OK) {
        xSemaphoreGive(device_in_use_mutex);
        ESP_LOGE(TAG, "Could not write measure_air_quality");
        return got_sent;
    }
    // The sensor needs a max of 12 ms to respond to the I2C read header.
    vTaskDelay(pdMS_TO_TICKS(write_delay));

    xSemaphoreGive(device_in_use_mutex);


    return ESP_OK;
}

static esp_err_t sgp30_command_rw(
    i2c_master_dev_handle_t dev_handle,
    sgp30_register_rw_t command,
    uint8_t write_delay,
    uint8_t read_delay,
    uint16_t *response,
    size_t response_len)
{

    uint8_t command_buffer[2];
    command_buffer[0] = (command >> 8) & 0xFF; /* Mask should be unnecessary */
    command_buffer[1] = command & 0xFF; /* Mask should be unnecessary */

    // We need 2 bytes for each word and an extra one for the CRC
    size_t response_buffer_len = response_len * 3;
    uint8_t response_buffer[response_buffer_len];

    /* We return 2 words (16 bits/w) each followed by an 8 bit CRC checksum */

    // We need to make sure no two threads/processes attempt to interact with
    // the device at the same time or in its calculation times.
    xSemaphoreTake(device_in_use_mutex, portMAX_DELAY);

    esp_err_t got_sent = i2c_master_transmit(dev_handle, command_buffer, 2, -1);
    if (got_sent != ESP_OK) {
        xSemaphoreGive(device_in_use_mutex);
        ESP_LOGE(TAG, "Could not write measure_air_quality");
        return got_sent;
    }
    // The sensor needs a max of 12 ms to respond to the I2C read header.
    vTaskDelay(pdMS_TO_TICKS(write_delay));

    esp_err_t got_received = i2c_master_receive(dev_handle, response_buffer, response_buffer_len, -1);
    if (got_received != ESP_OK) {
        xSemaphoreGive(device_in_use_mutex);
        ESP_LOGE(TAG, "Could not write measure_air_quality");
        return got_received;
    }

    xSemaphoreGive(device_in_use_mutex);

    // Check received CRC's
    for (int i = 0; i < response_len; i++) {
        ESP_RETURN_ON_ERROR(
            crc8_check(response_buffer + 3 * i, 3),
            TAG, "I2C get %d item CRC failed", i
        );
    }

    // Store the output into a uint16
    for (int i = 0; i < response_len; i++) {
        response[i] = (uint16_t) (response_buffer[i * 3] << 8) | response_buffer[3 * i + 1];
    }

    return ESP_OK;
}

static void sgp30_get_baseline_callback(void* args) {
    ESP_ERROR_CHECK(sgp30_get_baseline());
    esp_timer_start_periodic(g_h_baseline_1h_timer, 3600000000);
}
static void sgp30_update_baseline_callback(void* args) {
    ESP_ERROR_CHECK(sgp30_get_baseline());
}
static void sgp30_signal_initialized_callback(void* args) {
    sgp30_is_initialized = true;
    ESP_ERROR_CHECK(
        esp_event_post_to(
            sgp30_event_loop,
            SENSOR_EVENTS,
            SENSOR_IAQ_INITIALIZED,
            NULL,
            0,
            portMAX_DELAY)
    );
    // What if we cant post it for some reason? how do i handle this?
}

static void sgp30_measurement_callback(void* args) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(sgp30_measure_air_quality());
}

esp_err_t sgp30_init(esp_event_loop_handle_t loop) {
    sgp30_event_loop = loop;

    esp_timer_create_args_t baseline_1h_timer_args = {
        .name = "baseline_timer",
        .callback = sgp30_update_baseline_callback,
    };

    ESP_RETURN_ON_ERROR(
        esp_timer_create(
            &baseline_1h_timer_args,
            &g_h_baseline_1h_timer),
        TAG, "Could not initiate baseline timer"
    );

    esp_timer_create_args_t baseline_12h_timer_args = {
        .name = "baseline_timer",
        .callback = sgp30_get_baseline_callback,
    };

    ESP_RETURN_ON_ERROR(
        esp_timer_create(
            &baseline_12h_timer_args,
            &g_h_baseline_12h_timer),
        TAG, "Could not initiate baseline timer"
    );

    esp_timer_create_args_t initializing_timer_args = {
        .name = "initialization_timer",
        .callback = sgp30_signal_initialized_callback,
    };

    ESP_RETURN_ON_ERROR(
        esp_timer_create(
            &initializing_timer_args,
            &g_h_initialization_timer
        ),
        TAG, "Could not initiate init timer"
    );

    esp_timer_create_args_t measuring_timer_args = {
        .name = "measuring_timer",
        .callback = sgp30_measurement_callback,
    };

    ESP_RETURN_ON_ERROR(
        esp_timer_create(
            &measuring_timer_args,
            &g_h_measurement_timer
        ),
        TAG, "Could not initiate measurement timer"
    );

    ESP_RETURN_ON_ERROR(
        sgp30_init_air_quality(),
        TAG, "Could not send initiation command"
    );

    if (g_baseline_is_set) {
        sgp30_set_baseline();
    } else {

        for (int i =0; i < 15; i++) {
            vTaskDelay(pdMS_TO_TICKS(998));
            sgp30_measure_air_quality();
        }
        ESP_LOGI(TAG, "Initialization loop done");

        sgp30_get_baseline();
        sgp30_set_baseline();
    }

    esp_timer_start_once(g_h_initialization_timer, 2 * 1000 * 1000);


    return ESP_OK;
}

esp_err_t sgp30_delete() {
    // Remove dangling pointer to possible extern event loop.
    sgp30_event_loop = NULL;
    return ESP_OK;
}

esp_err_t sgp30_device_create(
    i2c_master_bus_handle_t bus_handle,
    const uint16_t dev_addr,
    const uint32_t dev_speed)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = dev_speed,
    };

    // Add device to the I2C bus
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(bus_handle, &dev_cfg, &sgp30_dev_handle),
        TAG, "Could not add device to I2C bus"
    );

    vSemaphoreCreateBinary(device_in_use_mutex);

    return ESP_OK;
}

esp_err_t sgp30_device_delete(i2c_master_dev_handle_t dev_handle)
{
    vSemaphoreDelete(device_in_use_mutex);
    return i2c_master_bus_rm_device(dev_handle);
}

esp_err_t sgp30_start_measuring()
{
    ESP_RETURN_ON_ERROR(
        esp_timer_start_periodic(
            g_h_measurement_timer,
            SGP30_MEASURING_INTERVAL
        ),
        TAG, "Could not start measurement_timer"
    );
    return ESP_OK;
}

esp_err_t sgp30_init_air_quality()
{
    ESP_LOGI(TAG, "Initiating");
    // [TODO]
    // We should check nvs for a baseline and if is set we retrieve it.
    // If it is not set we set up a timer for 12h to read the baseline
    // and store it in nvs.
    ESP_RETURN_ON_ERROR(
        sgp30_command_w(
            sgp30_dev_handle,
            SGP30_REG_INIT_AIR_QUALITY,
            12,
            NULL,
            0),
        TAG, "Could not send INIT_AIR_QUALITY command"
    );

    ESP_RETURN_ON_ERROR(
        esp_event_post_to(
            sgp30_event_loop,
            SENSOR_EVENTS,
            SENSOR_IAQ_INITIALIZING,
            NULL,
            0,
            portMAX_DELAY),
        TAG, "Could not post INIT_AIR_QUALITY_EVENT"
    );

    return ESP_OK;
}

esp_err_t sgp30_measure_air_quality()
{
    // This function will return an invalid measurement if device is
    // setting a temporal baseline i.e. 15 secs after first boot.
    uint16_t response_buffer[2];

    // We need to make sure no two threads/processes attempt to interact with
    // the device at the same time or in its calculation times.

    ESP_RETURN_ON_ERROR(
        sgp30_command_rw(
            sgp30_dev_handle,
            SGP30_REG_MEASURE_AIR_QUALITY,
            25,
            13,
            response_buffer,
            2),
        TAG, "Could not execute MEASURE_AIR_QUALITY command"
    );

    // Event data will be copied and managed by the event system, it can be local here
    sgp30_measurement_t new_measurement;
    new_measurement.eCO2 = response_buffer[0];
    new_measurement.TVOC = response_buffer[1];

    ESP_RETURN_ON_ERROR(
        esp_event_post_to(
            sgp30_event_loop,
            SENSOR_EVENTS,
            SENSOR_EVENT_NEW_MEASUREMENT,
            &new_measurement,
            sizeof(sgp30_measurement_t),
            portMAX_DELAY
        ),
        TAG, "Could not post new measurement"
    );

    return ESP_OK;
}

esp_err_t sgp30_set_baseline()
{
    ESP_LOGI(TAG, "Setting baseline");
    uint16_t msg_buffer[2];
    msg_buffer[0] = baseline.eCO2;
    msg_buffer[1] = baseline.TVOC;

    ESP_RETURN_ON_ERROR(
        sgp30_command_w(
            sgp30_dev_handle,
            SGP30_REG_SET_BASELINE,
            13,
            msg_buffer,
            2),
        TAG, "Could not send SET_BASELINE command"
    );

    return ESP_OK;
}

esp_err_t sgp30_get_baseline()
{
    /* We return 2 words (16 bits/w) each followed by an 8 bit CRC checksum */
    /* Therefore we need to save 48 bits for the received data */
    uint16_t response[2];

    ESP_RETURN_ON_ERROR(
        sgp30_command_rw(
            sgp30_dev_handle,
            SGP30_REG_GET_BASELINE,
            12,
            12, response, 2),
        TAG, "Could not execute GET_BASELINE command"
    );

    // Copy the read ID to the provided id pointer
    baseline.eCO2 = response[0];

    // bytes_read[2] is CRC for the first word, we can ignore it.

    baseline.TVOC = response[1];
    // bytes_read[5] is CRC for the second word, we can ignore it.

    ESP_RETURN_ON_ERROR(
        esp_event_post_to(
            sgp30_event_loop,
            SENSOR_EVENTS,
            SENSOR_GOT_BASELINE,
            &baseline,
            sizeof(sgp30_measurement_t),
            portMAX_DELAY
        ),
        TAG, "Could not post new baseline event"
    );
    ESP_LOGI(TAG, "Got new baseline from device");

    return ESP_OK;
}

esp_err_t sgp30_get_id()
{
    uint16_t response[3];
    ESP_RETURN_ON_ERROR(
        sgp30_command_rw(
            sgp30_dev_handle,
            SGP30_REG_GET_SERIAL_ID,
            12,
            12,
            response,
            3
        ),
        TAG, "I2C get serial id write failed"
    );

    // The sensor needs .5 ms to respond to the I2C read header.
    vTaskDelay(pdMS_TO_TICKS(1));

    // Copy the read ID to the provided id pointer
    id[0] = response[0];
    id[1] = response[1];
    id[2] = response[2];

    return ESP_OK;
}
