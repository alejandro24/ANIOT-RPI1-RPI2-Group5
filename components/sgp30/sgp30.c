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

ESP_EVENT_DEFINE_BASE(SGP30_EVENT);

#define SGP30_MEASURING_INTERVAL (1 * 1000 * 1000) //Measure each second
#define SGP30_BASELINE_VALIDITY_TIME (60 * 60 * 1000 * 1000)
#define SGP30_BASELINE_UPDATE_INTERVAL (30 * 1000000)
#define SGP30_FIRST_BASELINE_WAIT_TIME (60 * 1000000)

static char* TAG = "SGP30";
static esp_event_loop_handle_t sgp30_event_loop;
static i2c_master_dev_handle_t sgp30_dev_handle;
static sgp30_aggregate_t sgp30_measurement_aggregate;
static esp_timer_handle_t sgp30_update_baseline_timer_handle;
static esp_timer_handle_t sgp30_acquire_baseline_timer_handle;
static sgp30_state_t sgp30_state;
static TaskHandle_t sgp30_operation_task_handle;
static esp_timer_handle_t sgp30_measurement_timer_handle;
static uint32_t g_measurement_interval;
static SemaphoreHandle_t g_measurement_interval_mutex;
static SemaphoreHandle_t device_in_use_mutex;
static uint16_t id[3];

void sgp30_on_new_measurement(
    void * handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    sgp30_measurement_t new_measurement;
    time_t now;
    sgp30_log_entry_t new_log_entry;
    time(&now);
    new_measurement = *((sgp30_measurement_t*) event_data);
    ESP_LOGI(TAG, "Measured eCO2= %d TVOC= %d", new_measurement.eCO2, new_measurement.TVOC);
    sgp30_measurement_to_log_entry(&new_measurement, &now, &new_log_entry);
    //sgp30_measurement_enqueue(&new_log_entry, &sgp30_log);
}

void sgp30_on_new_baseline(
    void * handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    time_t now;
    sgp30_log_entry_t new_baseline;
    time(&now);
    sgp30_measurement_to_log_entry(
        (sgp30_measurement_t*) event_data,
        &now,
        &new_baseline
    );

    ESP_LOGI(
        TAG,
        "Baseline eCO2= %d TVOC= %d at timestamp %s",
        new_baseline.measurements.eCO2,
        new_baseline.measurements.TVOC,
        ctime(&new_baseline.tv)
    );
}

void sgp30_operation_task(void *args) {
    uint32_t elapsed_secs = 0;
    sgp30_measurement_t *baseline_handle;
    baseline_handle = ((sgp30_measurement_t*) args);
    sgp30_measurement_t last_measurement;
    sgp30_measurement_t baseline;
    sgp30_state = SGP30_STATE_INITIALIZING;
    while(true) {
        // We wait for a signal from the timer or 1 sec as fallback
        elapsed_secs = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        switch (sgp30_state) {
            case SGP30_STATE_INITIALIZING:
                sgp30_measure_air_quality(sgp30_dev_handle, &last_measurement);
                if (last_measurement.eCO2 != 400) {
                    ESP_LOGE(TAG, "Wrong eCO2 returned");
                }
                if (last_measurement.TVOC != 0) {
                    ESP_LOGE(TAG, "Wrong TVOC returned");
                }
                if (elapsed_secs == 15) {
                    if (baseline_handle == NULL) {
                        sgp30_state = SGP30_STATE_BASELINE_ACQUISITION;
                    } else {
                        sgp30_set_baseline(sgp30_dev_handle, &baseline);
                        if (pdTRUE == xTaskNotifyStateClear(xTaskGetCurrentTaskHandle())) {
                            ESP_LOGE(TAG, "I dont know");
                        }
                        sgp30_state = SGP30_STATE_FUNCTIONING;
                    }
                }
                break;

            case SGP30_STATE_BASELINE_ACQUISITION:
                sgp30_measure_air_quality(sgp30_dev_handle, NULL);
                if (elapsed_secs >= SGP30_FIRST_BASELINE_WAIT_TIME) {
                    if (pdTRUE == xTaskNotifyStateClear(xTaskGetCurrentTaskHandle())) {
                        ESP_LOGE(TAG, "I dont know");
                    }
                    sgp30_get_baseline_and_post_esp_event();
                    sgp30_state = SGP30_STATE_FUNCTIONING;
                }
                break;

            case SGP30_STATE_FUNCTIONING:
                sgp30_measure_air_quality(sgp30_dev_handle, &last_measurement);
                sgp30_update_aggregate(&sgp30_measurement_aggregate, &last_measurement);
                if (elapsed_secs >= SGP30_BASELINE_UPDATE_INTERVAL) {
                    if (pdTRUE == xTaskNotifyStateClear(xTaskGetCurrentTaskHandle())) {
                        ESP_LOGE(TAG, "I dont know");
                    }
                    sgp30_get_baseline_and_post_esp_event();
                }
                break;

            default:
                ESP_LOGE(TAG, "Undefined");
                break;
        }
    }
}

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
    sgp30_register_w_t command,
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

static void sgp30_acquire_baseline_callback(void* args) {

    // [Unhandled]
    ESP_RETURN_VOID_ON_ERROR(
        sgp30_get_baseline_and_post_esp_event(),
        TAG,
        "Could not get baseline"
    );

    // [Unhandled]
    ESP_RETURN_VOID_ON_ERROR(
        esp_timer_start_periodic(
            sgp30_update_baseline_timer_handle,
            SGP30_BASELINE_UPDATE_INTERVAL),
        TAG,
        "Could not start timer"
    );
}

    // Store e output into a uint16
    for (int i = 0; i < response_len; i++) {
        response[i] = (uint16_t) (response_buffer[i * 3] << 8) | response_buffer[3 * i + 1];
     }

    return ESP_OK;
}

static void sgp30_update_baseline_callback(void* args) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(sgp30_get_baseline_and_post_esp_event());
}

void sgp30_on_new_interval(
    void * handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    uint32_t new_measurement_interval = *((uint32_t*) event_data);
    g_measurement_interval = new_measurement_interval;
}

esp_err_t sgp30_update_aggregate(
    sgp30_aggregate_t *aggregate,
    const sgp30_measurement_t *new_measurement
) {
    if (aggregate->count == 0) {
        aggregate->mean.eCO2 = new_measurement->eCO2;
        aggregate->mean.TVOC = new_measurement->TVOC;
        aggregate->count++;
    } else if (aggregate->count < g_measurement_interval) {
        //[WARN] maybe overflow!
        aggregate->mean.eCO2 = (new_measurement->eCO2 + aggregate->mean.eCO2 * aggregate->count) / (aggregate->count + 1);
        aggregate->mean.TVOC = (new_measurement->TVOC + aggregate->mean.TVOC * aggregate->count) / (aggregate->count + 1);
        aggregate->count++;
    } else {
        aggregate->count = 0;
        aggregate->mean.eCO2 = (new_measurement->eCO2 + aggregate->mean.eCO2 * aggregate->count) / (aggregate->count + 1);
        aggregate->mean.TVOC = (new_measurement->TVOC + aggregate->mean.TVOC * aggregate->count) / (aggregate->count + 1);
        ESP_RETURN_ON_ERROR(
            esp_event_post_to(
                sgp30_event_loop,
                SGP30_EVENT,
                SGP30_EVENT_NEW_MEASUREMENT,
                &aggregate->mean,
                sizeof(sgp30_measurement_t),
                portMAX_DELAY
            ),
            TAG, "Could not post new measurement"
        );
    }
}

esp_err_t sgp30_init(
    esp_event_loop_handle_t loop,
    sgp30_measurement_t *baseline
) {
    // Obtain the event loop from the main function
    sgp30_event_loop = loop;

    // Subscribe to handle NEW_INTERVAL events

    ESP_RETURN_ON_ERROR(
        sgp30_init_air_quality(),
        TAG, "Could not initiate sensor"
    );

    xTaskCreate(sgp30_operation_task, "sgp30 operation", 2048, baseline, 2, &sgp30_operation_task_handle);

    esp_timer_start_periodic(sgp30_measurement_timer_handle, 1 * 1000 * 1000);

    return ESP_OK;
}

esp_err_t sgp30_delete() {
    // [TODO]
    return ESP_OK;
}

// Created the device and allocate all needed structures.
esp_err_t sgp30_device_create(
    i2c_master_bus_handle_t bus_handle,
    const uint16_t dev_addr,
    const uint32_t dev_speed
) {
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
    // Create the Mutex for access to the device
    // this will prevent asyncronous access to the resource
    // resulting in undefined behaviour.
    device_in_use_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(device_in_use_mutex, ESP_ERR_NO_MEM, TAG, "Could not create SGP30 mutex");

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
            sgp30_measurement_timer_handle,
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
            SGP30_EVENT,
            SGP30_EVENT_IAQ_INITIALIZING,
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
<<<<<<< HEAD
            13,
=======
            12,
>>>>>>> nvs-impl
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
            SGP30_EVENT,
            SGP30_EVENT_NEW_MEASUREMENT,
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
            SGP30_EVENT,
            SGP30_EVENT_NEW_BASELINE,
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
