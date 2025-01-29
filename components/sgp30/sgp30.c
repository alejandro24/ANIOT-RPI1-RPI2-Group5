#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "portmacro.h"
#include "sgp30.h"
#include "sgp30_types.h"
#include <stdint.h>
#include <string.h>
#include <time.h>

ESP_EVENT_DEFINE_BASE (SGP30_EVENT);

#define MEASURE_IN_FIRST_BASELINE_WAIT_TIME
#define SGP30_MEASURING_INTERVAL       (1000 * 1000) /* Measure each second */
#define SGP30_BASELINE_VALIDITY_TIME   (7 * 24 * 60)
#define SGP30_BASELINE_UPDATE_INTERVAL (30 * 1000000)
#define SGP30_FIRST_BASELINE_WAIT_TIME (60 * 1000000)

static char *TAG = "SGP30";
static esp_event_loop_handle_t sgp30_event_loop;
static i2c_master_dev_handle_t sgp30_dev_handle;
static esp_timer_handle_t sgp30_req_measurement_timer_handle;
static sgp30_state_t sgp30_state;
static uint32_t sgp30_elapsed_secs;
static TaskHandle_t sgp30_operation_task_handle;
static esp_timer_handle_t sgp30_measurement_timer_handle;
static SemaphoreHandle_t sgp30_measurement_requested;
static SemaphoreHandle_t device_in_use_mutex;
static uint16_t id[3];

static void sgp30_request_measurement_callback ()
{
    ESP_LOGI(TAG, "Requested measurement");
    xSemaphoreGive (sgp30_measurement_requested);
}

static void sgp30_on_sec_callback (
    void *args
)
{
    xTaskNotifyGive (sgp30_operation_task_handle);
}

static esp_err_t sgp30_start ()
{
    ESP_RETURN_ON_ERROR (
        esp_timer_start_periodic (
            sgp30_measurement_timer_handle,
            SGP30_MEASURING_INTERVAL
        ),
        TAG,
        "Could not start measurement_timer"
    );
    return ESP_OK;
}

static void sgp30_operation_task (
    void *args
)
{
    sgp30_measurement_t *baseline_handle = ((sgp30_measurement_t *)args);
    sgp30_measurement_t last_measurement;
    sgp30_measurement_log_t measurement_log;
    sgp30_measurement_t baseline;
    sgp30_state = SGP30_STATE_UNINITIAZED;
    ESP_ERROR_CHECK (sgp30_start ());
    while (true)
    {
        /* We wait for a signal from the timer or 1 sec as fallback*/
        ulTaskNotifyTake (pdTRUE, pdMS_TO_TICKS (10000));
        sgp30_elapsed_secs++;
        ESP_LOGI (TAG, "%" PRIu32 " seconds elapsed", sgp30_elapsed_secs);
        /* state_operation[sgp30_state].operation();*/
        switch (sgp30_state)
        {
        case SGP30_STATE_UNINITIAZED:
            sgp30_init_air_quality (sgp30_dev_handle);
            sgp30_elapsed_secs = 0;
            sgp30_state = SGP30_STATE_INITIALIZING;
            break;

        case SGP30_STATE_INITIALIZING:
            sgp30_measure_air_quality (sgp30_dev_handle, &last_measurement);
            if (last_measurement.eCO2 != 400)
            {
                ESP_LOGE (
                    TAG,
                    "Wrong eCO2 returned, got %d",
                    last_measurement.eCO2
                );
            }
            if (last_measurement.TVOC != 0)
            {
                ESP_LOGE (
                    TAG,
                    "Wrong TVOC returned, got %d",
                    last_measurement.TVOC
                );
            }
            if (sgp30_elapsed_secs == 15)
            {
                if (baseline_handle == NULL)
                {
                    sgp30_state = SGP30_STATE_BASELINE_ACQUISITION;
                }
                if (last_measurement.TVOC != 0)
                {
                    ESP_LOGE (
                        TAG,
                        "Wrong TVOC returned, got %d",
                        last_measurement.TVOC
                    );
                }
                if (sgp30_elapsed_secs == 15)
                {
                    if (baseline_handle == NULL)
                    {
                        sgp30_get_baseline (sgp30_dev_handle, &baseline);
                        sgp30_set_baseline (sgp30_dev_handle, &baseline);
                        sgp30_state = SGP30_STATE_BASELINE_ACQUISITION;
                    }
                    else
                    {
                        sgp30_set_baseline (sgp30_dev_handle, baseline_handle);
                        sgp30_elapsed_secs = 0;
                        sgp30_state = SGP30_STATE_FUNCTIONING;
                    }
                }
                break;

            case SGP30_STATE_BASELINE_ACQUISITION:
                sgp30_measure_air_quality (
                    sgp30_dev_handle,
                    &last_measurement
                );
                sgp30_measurement_log_enqueue(
                    &last_measurement,
                    &measurement_log
                );
                ESP_LOGI (
                    TAG,
                    "Measured: eC02: %" PRIu16 "\tTVOC: %" PRIu16 "",
                    last_measurement.eCO2,
                    last_measurement.TVOC
                );
                if (sgp30_elapsed_secs >= SGP30_FIRST_BASELINE_WAIT_TIME)
                {
                    sgp30_elapsed_secs = 0;
                    sgp30_get_baseline_and_post_esp_event ();
                    sgp30_state = SGP30_STATE_FUNCTIONING;
                }
#ifdef MEASURE_IN_FIRST_BASELINE_WAIT_TIME
                if (xSemaphoreTake (sgp30_measurement_requested, 0) == pdTRUE)
                {
                    sgp30_measurement_t mean;
                    sgp30_measurement_log_get_mean(&mean, &measurement_log);
                    ESP_LOGI (
                        TAG,
                        "Mean: eC02: %" PRIu16 "\tTVOC: %" PRIu16 "",
                        mean.eCO2,
                        mean.TVOC
                    );
                    ESP_ERROR_CHECK (esp_event_post_to (
                        sgp30_event_loop,
                        SGP30_EVENT,
                        SGP30_EVENT_NEW_MEASUREMENT,
                        &mean,
                        sizeof(sgp30_measurement_t),
                        portMAX_DELAY
                    ));
                }
#endif
                break;

            case SGP30_STATE_FUNCTIONING:
                sgp30_measure_air_quality (
                    sgp30_dev_handle,
                    &last_measurement
                );
                sgp30_measurement_log_enqueue (
                    &last_measurement,
                    &measurement_log
                );
                /* We check if the semaphore is set without blocking*/
                if (xSemaphoreTake (sgp30_measurement_requested, 0) == pdTRUE)
                {
                    sgp30_measurement_t mean;
                    sgp30_measurement_log_get_mean(&mean, &measurement_log);
                    ESP_LOGI (
                        TAG,
                        "Mean: eC02: %" PRIu16 "\tTVOC: %" PRIu16 "",
                        mean.eCO2,
                        mean.TVOC
                    );
                    ESP_ERROR_CHECK (esp_event_post_to (
                        sgp30_event_loop,
                        SGP30_EVENT,
                        SGP30_EVENT_NEW_MEASUREMENT,
                        &mean,
                        sizeof (sgp30_measurement_t),
                        portMAX_DELAY
                    ));
                }
                else
                {
                    ESP_LOGI (TAG, "Semaphore Unavailable");
                }
                break;

            default:
                ESP_LOGE (TAG, "Undefined");
                break;
            }
        }
    }
}

static esp_err_t crc8_check (
    uint8_t *buffer,
    size_t size
)
{
    uint8_t crc = SGP30_CRC_8_INIT;

    for (int i = 0; i < size; i++)
    {
        /* We XOR the first/next bit into the crc*/
        crc ^= buffer[i];
        for (int j = 0; j < 8; j++)
        {
            /* Check if the MSB is set to 1*/
            if (crc & 0x80)
            {
                /* If it is set to 1 get rid of the 1 and XOR*/
                crc = (crc << 1) ^ SGP30_CRC_8_POLY;
            }
            else
            {
                /* If it is not, just keep looking for a 1*/
                crc <<= 1;
            }
        }
    }
    /* Error if reminder is not zero.*/
    if (crc)
    {
        return ESP_ERR_INVALID_CRC;
    }
    else
    {
        return ESP_OK;
    }
}

static uint8_t crc8_gen (
    uint8_t *buffer,
    size_t size
)
{
    uint8_t crc = SGP30_CRC_8_INIT;

    for (int i = 0; i < size; i++)
    {
        crc ^= buffer[i];
        for (int j = 0; j < 8; j++)
        {
            if ((crc & 0x80) != 0)
            {
                crc = (crc << 1) ^ SGP30_CRC_8_POLY;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static esp_err_t sgp30_execute_command (
    i2c_master_dev_handle_t dev_handle,
    sgp30_register_rw_t command,
    uint16_t *msg,
    size_t msg_len,
    uint8_t write_delay,
    uint16_t *response,
    size_t response_len,
    uint8_t read_delay
)
{
    size_t msg_buffer_len = 2 + 3 * msg_len;
    uint8_t msg_buffer[msg_buffer_len];
    msg_buffer[0] = (command >> 8) & 0xFF;
    msg_buffer[1] = command & 0xFF;

    for (int i = 0; i < msg_len; i++)
    {
        msg_buffer[3 * i + 2] = (msg[i] >> 8) & 0xFF;
        msg_buffer[3 * i + 3] = msg[i] & 0xFF;
        msg_buffer[3 * i + 4] = crc8_gen (msg_buffer + 2 + 3 * i, 2);
    }

    /* We need 2 bytes for each word and an extra one for the CRC*/
    size_t response_buffer_len = response_len * 3;
    uint8_t response_buffer[response_buffer_len];

    /* We return 2 words (16 bits/w) each followed by an 8 bit CRC
       checksum */

    /* We need to make sure no two threads/processes attempt to
     interact with the device at the same time or in its calculation times.*/
    xSemaphoreTake (device_in_use_mutex, portMAX_DELAY);

    esp_err_t got_sent = i2c_master_transmit (dev_handle, msg_buffer, 2, -1);
    if (got_sent != ESP_OK)
    {
        xSemaphoreGive (device_in_use_mutex);
        ESP_LOGE (TAG, "Could not write %x", command);
        return got_sent;
    }
    /* The sensor needs a max of 12 ms to respond to the I2C read header.*/
    vTaskDelay (pdMS_TO_TICKS (write_delay));

    if (response_buffer_len != 0)
    {
        esp_err_t got_received = i2c_master_receive (
            dev_handle,
            response_buffer,
            response_buffer_len,
            -1
        );
        if (got_received != ESP_OK)
        {
            xSemaphoreGive (device_in_use_mutex);
            ESP_LOGE (TAG, "Could not read %x", command);
            return got_received;
        }

        xSemaphoreGive (device_in_use_mutex);

        /* Check received CRC's*/
        for (int i = 0; i < response_len; i++)
        {
            ESP_RETURN_ON_ERROR (
                crc8_check (response_buffer + 3 * i, 3),
                TAG,
                "I2C get %d item CRC failed",
                i
            );
        }
        vTaskDelay (pdMS_TO_TICKS (read_delay));
        /* Store the output into a uint16*/
        for (int i = 0; i < response_len; i++)
        {
            response[i] = (uint16_t)(response_buffer[i * 3] << 8)
                          | response_buffer[3 * i + 1];
        }
    }
    else
    {
        xSemaphoreGive (device_in_use_mutex);
    }

    return ESP_OK;
}

esp_err_t sgp30_init (
    esp_event_loop_handle_t loop,
    sgp30_measurement_t *baseline
)
{
    /* Obtain the event loop from the main function*/
    sgp30_event_loop = loop;

    sgp30_measurement_requested = xSemaphoreCreateBinary ();

    /* Set up a timer each second*/
    esp_timer_create_args_t measurement_timer_args = {
        .callback = sgp30_on_sec_callback,
        .name = "each_second"
    };

    ESP_ERROR_CHECK (esp_timer_create (
        &measurement_timer_args,
        &sgp30_measurement_timer_handle
    ));
    /* Set up a timer to send data*/
    esp_timer_create_args_t sgp30_req_measurement_timer_args = {
        .callback = sgp30_request_measurement_callback,
        .name = "request_measurement"
    };

    esp_timer_create (
        &sgp30_req_measurement_timer_args,
        &sgp30_req_measurement_timer_handle
    );

    xTaskCreate (
        sgp30_operation_task,
        "sgp30_operation",
        4096,
        baseline,
        2,
        &sgp30_operation_task_handle
    );
    if (sgp30_operation_task_handle == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t sgp30_delete ()
{
    /* [TODO]*/
    return ESP_OK;
}

/* Created the device and allocate all needed structures.*/
esp_err_t sgp30_device_create (
    i2c_master_bus_handle_t bus_handle,
    const uint16_t dev_addr,
    const uint32_t dev_speed
)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = dev_speed,
    };

    /* Add device to the I2C bus*/

    ESP_RETURN_ON_ERROR (
        i2c_master_bus_add_device (bus_handle, &dev_cfg, &sgp30_dev_handle),
        TAG,
        "Could not add device to I2C bus"
    );
    /* Create the Mutex for access to the device
     this will prevent asyncronous access to the resource
     resulting in undefined behaviour.*/
    device_in_use_mutex = xSemaphoreCreateMutex ();
    ESP_RETURN_ON_FALSE (
        device_in_use_mutex,
        ESP_ERR_NO_MEM,
        TAG,
        "Could not create SGP30 mutex"
    );

    return ESP_OK;
}

esp_err_t sgp30_device_delete (
    i2c_master_dev_handle_t dev_handle
)
{
    vSemaphoreDelete (device_in_use_mutex);
    return i2c_master_bus_rm_device (dev_handle);
}

esp_err_t sgp30_start_measuring(
    uint32_t s
)
{
    return esp_timer_start_periodic (sgp30_req_measurement_timer_handle, ((uint64_t) s) * 1000000);
}
esp_err_t sgp30_restart_measuring (
    uint32_t s
)
{
    uint64_t new_measurement_interval = ((uint64_t) s) * 1000000;
    if (esp_timer_is_active (sgp30_req_measurement_timer_handle))
    {
        return esp_timer_restart (
            sgp30_req_measurement_timer_handle,
            new_measurement_interval
        );
    }
    else
    {
        return esp_timer_start_periodic (
            sgp30_req_measurement_timer_handle,
            new_measurement_interval
        );
    }
}

esp_err_t sgp30_init_air_quality (
    i2c_master_dev_handle_t dev_handle
)
{
    ESP_LOGI (TAG, "Initiating");
    ESP_RETURN_ON_ERROR (
        sgp30_execute_command (
            dev_handle,
            SGP30_REG_INIT_AIR_QUALITY,
            NULL,
            0,
            12,
            NULL,
            0,
            0
        ),
        TAG,
        "Could not send INIT_AIR_QUALITY command"
    );

    /* ESP_RETURN_ON_ERROR(
         esp_event_post_to(
             sgp30_event_loop,
             SGP30_EVENT,
             SGP30_EVENT_IAQ_INITIALIZING,
             NULL,
             0,
             portMAX_DELAY),
         TAG, "Could not post SENSOR_EVENT_IAQ_INITIALIZING"
     );*/

    return ESP_OK;
}

esp_err_t sgp30_measure_air_quality (
    i2c_master_dev_handle_t dev_handle,
    sgp30_measurement_t *air_quality
)
{
    uint16_t response_buffer[2];

    ESP_RETURN_ON_ERROR (
        sgp30_execute_command (
            dev_handle,
            SGP30_REG_MEASURE_AIR_QUALITY,
            NULL,
            0,
            25,
            response_buffer,
            2,
            12
        ),
        TAG,
        "Could not execute MEASURE_AIR_QUALITY command"
    );

    if (air_quality == NULL)
    {
        return ESP_OK;
    }
    else
    {
        air_quality->eCO2 = response_buffer[0];
        air_quality->TVOC = response_buffer[1];
        return ESP_OK;
    }
}

esp_err_t sgp30_measure_air_quality_and_post_esp_event ()
{
    sgp30_measurement_t new_measurement;

    ESP_RETURN_ON_ERROR (
        sgp30_measure_air_quality (sgp30_dev_handle, &new_measurement),
        TAG,
        "Could not get new measurement"
    );

    ESP_RETURN_ON_ERROR (
        esp_event_post_to (
            sgp30_event_loop,
            SGP30_EVENT,
            SGP30_EVENT_NEW_MEASUREMENT,
            &new_measurement,
            sizeof (sgp30_measurement_t),
            portMAX_DELAY
        ),
        TAG,
        "Could not post new measurement"
    );

    return ESP_OK;
}

esp_err_t sgp30_set_baseline (
    i2c_master_dev_handle_t dev_handle,
    const sgp30_measurement_t *new_baseline
)
{
    ESP_LOGI (TAG, "Setting baseline");
    uint16_t msg_buffer[2];
    msg_buffer[0] = new_baseline->eCO2;
    msg_buffer[1] = new_baseline->TVOC;

    ESP_RETURN_ON_ERROR (
        sgp30_execute_command (
            dev_handle,
            SGP30_REG_SET_BASELINE,
            msg_buffer,
            2,
            13,
            msg_buffer,
            2,
            0
        ),
        TAG,
        "Could not send SET_BASELINE command"
    );

    return ESP_OK;
}

esp_err_t sgp30_get_baseline (
    i2c_master_dev_handle_t dev_handle,
    sgp30_measurement_t *new_baseline
)
{
    uint16_t response[2];

    ESP_RETURN_ON_ERROR (
        sgp30_execute_command (
            dev_handle,
            SGP30_REG_GET_BASELINE,
            NULL,
            0,
            20,
            response,
            2,
            12
        ),
        TAG,
        "Could not execute GET_BASELINE command"
    );

    new_baseline->eCO2 = response[0];
    new_baseline->TVOC = response[1];

    return ESP_OK;
}

esp_err_t sgp30_get_baseline_and_post_esp_event ()
{
    sgp30_measurement_t tmp_baseline;
    ESP_RETURN_ON_ERROR (
        sgp30_get_baseline(sgp30_dev_handle, &tmp_baseline),
        TAG,
        "Could not get new baseline"
    );

    ESP_RETURN_ON_ERROR (
        esp_event_post_to (
            sgp30_event_loop,
            SGP30_EVENT,
            SGP30_EVENT_NEW_BASELINE,
            &tmp_baseline,
            sizeof (sgp30_measurement_t),
            portMAX_DELAY
        ),
        TAG,
        "Could not post new baseline event"
    );

    return ESP_OK;
}

esp_err_t sgp30_get_id ()
{
    uint16_t response[3];
    ESP_RETURN_ON_ERROR (
        sgp30_execute_command (
            sgp30_dev_handle,
            SGP30_REG_GET_SERIAL_ID,
            NULL,
            0,
            12,
            response,
            3,
            12
        ),
        TAG,
        "I2C get serial id write failed"
    );

    /* The sensor needs .5 ms to respond to the I2C read header.*/
    vTaskDelay (pdMS_TO_TICKS (1));

    /* Copy the read ID to the provided id pointer*/
    id[0] = response[0];
    id[1] = response[1];
    id[2] = response[2];

    return ESP_OK;
}

bool sgp30_is_baseline_expired (
    const time_t stored_time,
    const time_t curr_time
)
{
    if (stored_time + SGP30_BASELINE_VALIDITY_TIME > curr_time)
    {
        return true;
    }
    else
    {
        return false;
    }
}

esp_err_t sgp30_measurement_log_enqueue (
    const sgp30_measurement_t *m,
    sgp30_measurement_log_t *q
)
{
    if (q->size == MAX_QUEUE_SIZE)
    {
        q->measurements[q->oldest_index].eCO2 = m->eCO2;
        q->measurements[q->oldest_index].TVOC = m->TVOC;
        q->oldest_index = (q->oldest_index + 1) % MAX_QUEUE_SIZE;
    }
    else
    {
        q->measurements[(q->oldest_index + q->size) % MAX_QUEUE_SIZE].eCO2 = m->eCO2;
        q->measurements[(q->oldest_index + q->size) % MAX_QUEUE_SIZE].TVOC = m->TVOC;
        q->size++;
    }
    return ESP_OK;
}

esp_err_t sgp30_measurement_log_dequeue (
    sgp30_measurement_t *m,
    sgp30_measurement_log_t *q
)
{
    if (q->size == 0)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    else
    {
        q->oldest_index = (q->oldest_index + 1) % MAX_QUEUE_SIZE;
        q->size--;
        return ESP_OK;
    }
}

esp_err_t sgp30_measurement_log_get_mean (
    sgp30_measurement_t *m,
    const sgp30_measurement_log_t *q
)
{
    uint32_t mean_eCO2 = 0;
    uint32_t mean_TVOC = 0;
    int j = q->oldest_index;
    for (int i = 0; i < q->size; i++)
    {
        mean_eCO2 += (uint32_t)q->measurements[(j + i) % MAX_QUEUE_SIZE].eCO2;
        mean_TVOC += (uint32_t)q->measurements[(j + i) % MAX_QUEUE_SIZE].TVOC;
    }
    m->eCO2 = (uint16_t)(mean_eCO2 / q->size);
    m->TVOC = (uint16_t)(mean_TVOC / q->size);
    return ESP_OK;
}
