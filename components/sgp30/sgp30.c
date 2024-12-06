#include "driver/i2c_types.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_event_base.h"
#include "esp_err.h"
#include "esp_log.h"
#include "portmacro.h"
#include <stdint.h>
#include <stdlib.h>
#include "sgp30.h"

ESP_EVENT_DEFINE_BASE(SENSOR_EVENTS);

static char* TAG = "SGP30";
static esp_event_loop_handle_t sgp30_event_loop;
static i2c_master_dev_handle_t sgp30_dev_handle;
static uint32_t g_baseline;
static bool g_baseline_is_set = false;
static bool sgp30_is_initialized = false;
static esp_timer_handle_t g_h_baseline_12h_timer;
static esp_timer_handle_t g_h_baseline_1h_timer;
static esp_timer_handle_t g_h_initialization_timer;
static esp_timer_handle_t g_h_measurement_timer;
// This is a very inefficient
static esp_err_t crc8_check(uint8_t *buffer, size_t size) {
    uint8_t crc = 0;

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
    uint8_t crc = 0;

    for (int i = 0; i < size; i++) {
        crc ^= buffer[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ SGP30_CRC_8_POLY;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}
static void sgp30_get_baseline_callback(void* args) {
    ESP_ERROR_CHECK(sgp30_get_baseline());
    esp_timer_start_periodic(g_h_baseline_1h_timer, 1 * 60 * 60 * 1000 * 1000);
}
static void sgp30_update_baseline_callback(void* args) {
    ESP_ERROR_CHECK(sgp30_get_baseline());
}
static void sgp30_signal_initialized_callback(void* args) {
    // What if we cant post it for some reason? how do i handle this?
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

    return ESP_OK;
}

esp_err_t sgp30_delete() {
    // Remove dangling pointer to possible extern event loop.
    sgp30_event_loop = NULL;
    return ESP_OK;
}

esp_err_t gp30_device_create(
    i2c_master_bus_handle_t bus_handle,
    const uint16_t dev_addr,
    const uint32_t dev_speed)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = dev_speed,
    };

    i2c_master_dev_handle_t dev_handle;

    // Add device to the I2C bus
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle),
        TAG, "Could not add device to I2C bus"
    );

    return ESP_OK;
}

esp_err_t sgp30_device_delete(i2c_master_dev_handle_t dev_handle)
{
    return i2c_master_bus_rm_device(dev_handle);
}

esp_err_t sgp30_init_air_quality()
{
    // [TODO]
    // We should check nvs for a baseline and if is set we retrieve it.
    // If it is not set we set up a timer for 12h to read the baseline
    // and store it in nvs.
    sgp30_register_rw_t write_addr = SGP30_REG_INIT_AIR_QUALITY;
    uint8_t write_buffer[2];
    write_buffer[0] = write_addr & 0xFF; /* Mask should be unnecessary */
    write_buffer[1] = (write_addr >> 8) & 0xFF; /* Mask should be unnecessary */

    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(sgp30_dev_handle, write_buffer, 2, -1),
        TAG, "I2C init air quality failed"
    );

    // The sensor needs a max of 10 ms to respond to the I2C read header.
    vTaskDelay(pdMS_TO_TICKS(1));

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
    esp_timer_start_once(g_h_initialization_timer, 15 * 1000 * 1000);

    return ESP_OK;
}

esp_err_t sgp30_measure_air_quality()
{
    // This function will return an invalid measurement if device is
    // setting a temporal baseline i.e. 15 secs after first boot.
    sgp30_register_rw_t write_addr = SGP30_REG_MEASURE_AIR_QUALITY;
    uint8_t write_buffer[2];
    write_buffer[0] = write_addr & 0xFF; /* Mask should be unnecessary */
    write_buffer[1] = (write_addr >> 8) & 0xFF; /* Mask should be unnecessary */

    /* We return 2 words (16 bits/w) each followed by an 8 bit CRC checksum */
    /* Therefore we need to save 48 bits for the received data */
    uint8_t bytes_read[6];

    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(sgp30_dev_handle, write_buffer, 2, -1),
        TAG, "I2C get serial id write failed"
    );

    // The sensor needs a max of 12 ms to respond to the I2C read header.
    vTaskDelay(pdMS_TO_TICKS(12));

    ESP_RETURN_ON_ERROR(
        i2c_master_receive(sgp30_dev_handle, bytes_read, 6, -1),
        TAG, "I2C get serial id read failed"
    );

    ESP_RETURN_ON_ERROR(
        crc8_check(bytes_read, 6),
        TAG, "I2C get serial id CRC failed"
    );

    sgp30_measurement_t *new_measurement = malloc(sizeof(sgp30_measurement_t));
    // Copy the read ID to the provided id pointer
    new_measurement->eCO2 = (uint16_t) (bytes_read[1] << 8) | bytes_read[0];
    // bytes_read[2] is CRC for the first word, we can ignore it.

    new_measurement->TVOC = (uint16_t) (bytes_read[4] << 8) | bytes_read[3];
    // bytes_read[5] is CRC for the second word, we can ignore it.

    ESP_RETURN_ON_ERROR(
        esp_event_post_to(
            sgp30_event_loop,
            SENSOR_EVENTS,
            SENSOR_EVENT_NEW_MEASUREMENT,
            new_measurement,
            sizeof(sgp30_measurement_t),
            portMAX_DELAY
        ),
        TAG, "Could not post new measurement"
    );

    return ESP_OK;
}

esp_err_t sgp30_get_baseline()
{
    sgp30_register_rw_t write_addr = SGP30_REG_GET_BASELINE;
    uint8_t write_buffer[2];
    write_buffer[0] = write_addr & 0xFF; /* Mask should be unnecessary */
    write_buffer[1] = (write_addr >> 8) & 0xFF; /* Mask should be unnecessary */

    /* We return 2 words (16 bits/w) each followed by an 8 bit CRC checksum */
    /* Therefore we need to save 48 bits for the received data */
    uint8_t bytes_read[6];

    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(sgp30_dev_handle, write_buffer, 2, -1),
        TAG, "I2C get serial id write failed"
    );

    // The sensor needs 10 ms to respond to the I2C read header.
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_RETURN_ON_ERROR(
        i2c_master_receive(sgp30_dev_handle, bytes_read, 9, -1),
        TAG, "I2C get serial id read failed"
    );

    ESP_RETURN_ON_ERROR(
        crc8_check(bytes_read, 6),
        TAG, "I2C get serial id CRC failed"
    );

    sgp30_baseline_t *new_baseline = malloc(sizeof(sgp30_baseline_t));
    // Copy the read ID to the provided id pointer
    new_baseline->baseline.eCO2 = (uint16_t) (bytes_read[1] << 8) | bytes_read[0];
    // bytes_read[2] is CRC for the first word, we can ignore it.

    new_baseline->baseline.TVOC = (uint16_t) (bytes_read[4] << 8) | bytes_read[3];
    // bytes_read[5] is CRC for the second word, we can ignore it.
    new_baseline->timestamp = 0; // [TODO] need to get the time!!

    ESP_RETURN_ON_ERROR(
        esp_event_post_to(
            sgp30_event_loop,
            SENSOR_EVENTS,
            SENSOR_GOT_BASELINE,
            new_baseline,
            sizeof(sgp30_baseline_t),
            portMAX_DELAY
        ),
        TAG, "Could not post new measurement"
    );

    return ESP_OK;
}

esp_err_t sgp30_get_id(i2c_master_dev_handle_t dev_handle, uint8_t *id)
{
    sgp30_register_rw_t write_addr = SGP30_REG_GET_SERIAL_ID;
    uint8_t write_buffer[2];
    write_buffer[0] = write_addr & 0xFF; /* Mask should be unnecessary */
    write_buffer[1] = (write_addr >> 8) & 0xFF; /* Mask should be unnecessary */

    /* We return 3 words (16 bits/w) each followed by an 8 bit CRC checksum */
    /* Therefore we need to save 72 bits for the received data */
    uint8_t bytes_read[9];

    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(dev_handle, write_buffer, 2, -1),
        TAG, "I2C get serial id write failed"
    );

    // The sensor needs .5 ms to respond to the I2C read header.
    vTaskDelay(pdMS_TO_TICKS(1));

    ESP_RETURN_ON_ERROR(
        i2c_master_receive(dev_handle, bytes_read, 9, -1),
        TAG, "I2C get serial id read failed"
    );

    ESP_RETURN_ON_ERROR(
        crc8_check(bytes_read, 9),
        TAG, "I2C get serial id CRC failed"
    );

    // Copy the read ID to the provided id pointer
    id[0] = bytes_read[0];
    id[1] = bytes_read[1];
    // bytes_read[2] is CRC for the first word, we can ignore it.

    id[2] = bytes_read[3];
    id[3] = bytes_read[4];
    // bytes_read[5] is CRC for the second word, we can ignore it.

    id[4] = bytes_read[6];
    id[5] = bytes_read[7];
    // bytes_read[8] is CRC for the third word, we can ignore it.

    return ESP_OK;
}
