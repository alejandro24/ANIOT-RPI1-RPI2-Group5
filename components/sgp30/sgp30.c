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
#include <string.h>
#include <time.h>
#include "sgp30.h"

ESP_EVENT_DEFINE_BASE(SENSOR_EVENTS);

#define SGP30_MEASURING_INTERVAL (1 * 1000 * 1000)
#define SGP30_BASELINE_VALIDITY_TIME (60 * 60 * 1000 * 1000)
#define SGP30_BASELINE_UPDATE_INTERVAL (30 * 1000000)
#define SGP30_FIRST_BASELINE_WAIT_TIME (60 * 1000000)

static char* TAG = "SGP30";
static esp_event_loop_handle_t sgp30_event_loop;
static i2c_master_dev_handle_t sgp30_dev_handle;
static esp_timer_handle_t g_update_baseline_timer_handle;
static bool g_sgp30_has_baseline;
static esp_timer_handle_t g_measurement_timer_handle;
static SemaphoreHandle_t device_in_use_mutex;
static uint16_t id[3];

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

    esp_err_t got_sent = i2c_master_transmit(dev_handle, command_buffer, 2, 1000);
    if (got_sent != ESP_OK) {
        xSemaphoreGive(device_in_use_mutex);
        ESP_LOGE(TAG, "Could not write %x", command);
        return got_sent;
    }
    // The sensor needs a max of 12 ms to respond to the I2C read header.
    vTaskDelay(pdMS_TO_TICKS(write_delay));

    ESP_LOGI(TAG, "wrote measurement cmd");
    esp_err_t got_received = i2c_master_receive(dev_handle, response_buffer, response_buffer_len, -1);
    if (got_received != ESP_OK) {
        xSemaphoreGive(device_in_use_mutex);
        ESP_LOGE(TAG, "Could not read %x", command);
        return got_received;
    }
    ESP_LOGI(TAG, "read measurement");

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

static esp_err_t sgp30_set_or_update_baseline_interval(
) {
    uint64_t new_baseline_interval = (g_sgp30_has_baseline) ?
        SGP30_FIRST_BASELINE_WAIT_TIME:
        SGP30_BASELINE_UPDATE_INTERVAL;
    if (esp_timer_is_active(g_update_baseline_timer_handle)) {
        ESP_RETURN_ON_ERROR(
            esp_timer_restart(
                g_update_baseline_timer_handle,
                new_baseline_interval
            ),
            TAG, "Could not update baseline timer"
        );
    } else {
        ESP_RETURN_ON_ERROR(
            esp_timer_start_once(
                g_update_baseline_timer_handle,
                new_baseline_interval
            ),
            TAG, "Could not start baseline timer"
        );
    }

    return ESP_OK;
}

static void sgp30_update_baseline_callback(void* args) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(sgp30_get_baseline_and_post_esp_event());
    g_sgp30_has_baseline = true;
    ESP_ERROR_CHECK(sgp30_set_or_update_baseline_interval());
}

static void sgp30_update_measurement_interval_event_handler(
    void * handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    uint64_t new_measurement_interval = *((uint64_t*) event_data);
    if (esp_timer_is_active(g_update_baseline_timer_handle)) {
        ESP_ERROR_CHECK(
            esp_timer_restart(
                g_measurement_timer_handle,
                new_measurement_interval
            )
        );
    } else {
        ESP_ERROR_CHECK(
            esp_timer_start_periodic(
                g_measurement_timer_handle,
                new_measurement_interval
            )
        );
    }
}

static void sgp30_measurement_callback(void* args) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(sgp30_measure_air_quality_and_post_esp_event());
}

esp_err_t sgp30_init(
    esp_event_loop_handle_t loop,
    sgp30_measurement_t *baseline
) {
    sgp30_event_loop = loop;


    ESP_RETURN_ON_ERROR(
        sgp30_init_air_quality(),
        TAG, "Could not initiate sensor"
    );

    if (baseline == NULL) {
        g_sgp30_has_baseline = false;
        ESP_LOGI(TAG, "Baseline not in flash");
        // If no baseline was stored do an initiation loop
        sgp30_measurement_t default_value;

        for (int i =0; i < 15; i++) {
            vTaskDelay(pdMS_TO_TICKS(998));
            sgp30_measure_air_quality(sgp30_dev_handle, &default_value);
            ESP_LOGI(TAG, "eC02 %d, TVOC %d", default_value.eCO2, default_value.TVOC);
            // ESP_RETURN_ON_FALSE((default_value.eCO2 == 400), ESP_ERR_INVALID_STATE, TAG, "got wrong eCO2 default value");
            // ESP_RETURN_ON_FALSE((default_value.TVOC == 0), ESP_ERR_INVALID_STATE, TAG, "got wrong TVOC devault value");
        }
        ESP_LOGI(TAG, "Initialization loop done");

//         sgp30_measurement_t tmp_baseline;
// 
//         sgp30_get_baseline(sgp30_dev_handle, &tmp_baseline);
//         sgp30_set_baseline(sgp30_dev_handle, &tmp_baseline);

        ESP_RETURN_ON_ERROR(
            sgp30_set_or_update_baseline_interval(),
            TAG, "Could not set or update baseline timer"
        );
    } else {
        g_sgp30_has_baseline = true;
        ESP_LOGI(TAG, "Baseline in flash");
        sgp30_set_baseline(sgp30_dev_handle, baseline);
        // and update it each hour
        // ESP_ERROR_CHECK(esp_timer_start_periodic(g_h_update_baseline_timer, 1 * 3600000000));
        ESP_RETURN_ON_ERROR(
            sgp30_set_or_update_baseline_interval(),
            TAG, "Could not set or update baseline timer"
        );
    }

    ESP_ERROR_CHECK(
        esp_event_post_to(
            sgp30_event_loop,
            SENSOR_EVENTS,
            SENSOR_EVENT_IAQ_INITIALIZED,
            NULL,
            0,
            portMAX_DELAY)
    );

    return ESP_OK;
}

esp_err_t sgp30_delete() {
    // [TODO]
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

    // Create the timer for measurements.

    esp_timer_create_args_t measuring_timer_args = {
        .name = "measuring_timer",
        .callback = sgp30_measurement_callback,
    };

    ESP_RETURN_ON_ERROR(
        esp_timer_create(
            &measuring_timer_args,
            &g_measurement_timer_handle
        ),
        TAG, "Could not create measurement timer"
    );

    // Create the timer for baseline update

    esp_timer_create_args_t update_baseline_timer_args = {
        .name = "baseline_timer",
        .callback = sgp30_update_baseline_callback,
    };

    ESP_RETURN_ON_ERROR(
        esp_timer_create(
            &update_baseline_timer_args,
            &g_update_baseline_timer_handle),
        TAG, "Could not create baseline timer"
    );

    // Create the Mutex for access to the device
    // this will prevent asyncronous access to the resource
    // resulting in undefined behaviour.

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
            g_measurement_timer_handle,
            SGP30_MEASURING_INTERVAL
        ),
        TAG, "Could not start measurement_timer"
    );
    return ESP_OK;
}

esp_err_t sgp30_init_air_quality()
{
    ESP_LOGI(TAG, "Initiating");
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
            SENSOR_EVENT_IAQ_INITIALIZING,
            NULL,
            0,
            portMAX_DELAY),
        TAG, "Could not post SENSOR_EVENT_IAQ_INITIALIZING"
    );

    return ESP_OK;
}


esp_err_t sgp30_measure_air_quality(i2c_master_dev_handle_t dev_handle, sgp30_measurement_t* air_quality)
{
    uint16_t response_buffer[2];

    ESP_RETURN_ON_ERROR(
        sgp30_command_rw(
            dev_handle,
            SGP30_REG_MEASURE_AIR_QUALITY,
            25,
            12,
            response_buffer,
            2
        ),
        TAG, "Could not execute MEASURE_AIR_QUALITY command"
    );

    air_quality->eCO2 = response_buffer[0];
    air_quality->TVOC = response_buffer[1];
    return ESP_OK;
}

esp_err_t sgp30_measure_air_quality_and_post_esp_event()
{
    sgp30_measurement_t new_measurement;

    ESP_RETURN_ON_ERROR(
        sgp30_measure_air_quality(
            sgp30_dev_handle,
            &new_measurement
        ),
        TAG, "Could not get new measurement"
    );

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

esp_err_t sgp30_set_baseline(
    i2c_master_dev_handle_t dev_handle,
    const sgp30_measurement_t* new_baseline
) {
    ESP_LOGI(TAG, "Setting baseline");
    uint16_t msg_buffer[2];
    msg_buffer[0] = new_baseline->eCO2;
    msg_buffer[1] = new_baseline->TVOC;

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


esp_err_t sgp30_get_baseline(
    i2c_master_dev_handle_t dev_handle,
    sgp30_measurement_t *new_baseline
) {
    uint16_t response[2];

    ESP_RETURN_ON_ERROR(
        sgp30_command_rw(
            sgp30_dev_handle,
            SGP30_REG_GET_BASELINE,
            12,
            12,
            response,
            2
        ),
        TAG, "Could not execute GET_BASELINE command"
    );

    new_baseline->eCO2 = response[0];
    new_baseline->TVOC = response[1];

    return ESP_OK;
}

esp_err_t sgp30_get_baseline_and_post_esp_event()
{
    sgp30_measurement_t tmp_baseline;
    ESP_RETURN_ON_ERROR(
        sgp30_get_baseline(
            sgp30_dev_handle,
            &tmp_baseline
        ),
        TAG, "Could not get new baseline"
    );

    ESP_RETURN_ON_ERROR(
        esp_event_post_to(
            sgp30_event_loop,
            SENSOR_EVENTS,
            SENSOR_EVENT_NEW_BASELINE,
            &tmp_baseline,
            sizeof(sgp30_measurement_t),
            portMAX_DELAY
        ),
        TAG, "Could not post new baseline event"
    );

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
esp_err_t sgp30_log_entry_to_valid_baseline_or_null(
    const sgp30_log_entry_t *in_log_entry,
    sgp30_measurement_t *out_measurement
) {
    time_t now;
    time(&now);
    if (in_log_entry->tv + SGP30_BASELINE_VALIDITY_TIME > now) {
        out_measurement->eCO2 = in_log_entry->measurements.eCO2;
        out_measurement->TVOC = in_log_entry->measurements.TVOC;
    } else {
        out_measurement = NULL;
    }
    return ESP_OK;
}
esp_err_t sgp30_measurement_to_log_entry(
    const sgp30_measurement_t *in_measurement,
    const time_t *now,
    sgp30_log_entry_t *out_log_entry
) {
    out_log_entry->measurements.eCO2 = in_measurement->eCO2;
    out_log_entry->measurements.TVOC = in_measurement->TVOC;
    out_log_entry->tv = *now;
    return ESP_OK;
}

esp_err_t sgp30_measurement_enqueue(
    const sgp30_log_entry_t *m,
    sgp30_log_t *q)
{
    if (q->size == MAX_QUEUE_SIZE) {
        return ESP_ERR_NO_MEM;
    } else {
        q->size++;
        memcpy(&q->measurements[(q->head + q->size) % MAX_QUEUE_SIZE], m, sizeof(sgp30_log_entry_t));
        return ESP_OK;
    }
}

esp_err_t sgp30_measurement_dequeue(
    sgp30_log_entry_t *m,
    sgp30_log_t *q)
{
    if (q->size == 0) {
        return ESP_ERR_INVALID_SIZE;
    } else {
        memcpy(m, &q->measurements[q->head], sizeof(sgp30_log_entry_t));
        #ifdef ZERO_OUT_QUEUE_ON_DEQUEUE
        memset(&q->measurements[q->head], 0, sizeof(sgp30_log_entry_t));
        #endif
        q->head = (q->head + 1) % MAX_QUEUE_SIZE;
        q->size--;
        return ESP_OK;
    }
}
