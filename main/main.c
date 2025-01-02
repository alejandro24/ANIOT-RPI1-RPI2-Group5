//#include "wifi_manager.h"
#include "driver/i2c_types.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "portmacro.h"
#include "sgp30.h"
#include "nvs_structures.h"
#include <stdint.h>
#include <string.h>
#include <nvs_flash.h>
#include "sntp_sync.h"  // Include the SNTP component
#include "wifi.h"
#include <stdio.h>

#define DEVICE_SDA_IO_NUM 21
#define DEVICE_SCL_IO_NUM 22

#define SGP30_STORAGE_NAMESPACE "sgp30"
#define SGP30_NVS_BASELINE_KEY "baseline"

static char* TAG = "MAIN";
// sgp30 required structures
i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t sgp30;
esp_event_loop_handle_t sgp30_event_loop_handle;
sgp30_log_t sgp30_log;
// nvs required structures
esp_event_loop_handle_t storage_event_loop_handle;
// Communication event loop
esp_event_loop_handle_t communication_event_loop_handle;


static void sgp30_on_new_measurement(
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
    sgp30_measurement_enqueue(&new_log_entry, &sgp30_log);
}

static void sgp30_on_iaq_initialized(
    void * handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    ESP_LOGI(TAG, "SGP30 Initialized.");
    sgp30_start_measuring();
}

static void sgp30_on_new_baseline(
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
static esp_err_t storage_invoke_get_baseline_command(
    QueueHandle_t storage_sgp30_baseline_ipc_handler,
    sgp30_log_entry_t *sgp30_baseline
) {
    if (xQueueReceive(storage_sgp30_baseline_ipc_handler, (void*) sgp30_baseline, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Unexpected error waiting to retrieve baseline from storage");
        return ESP_ERR_TIMEOUT;
    } else {
        return ESP_OK;
    }
}

esp_err_t init_i2c(void)
{
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = DEVICE_SCL_IO_NUM,
        .sda_io_num = DEVICE_SDA_IO_NUM,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_RETURN_ON_ERROR(
        i2c_new_master_bus(
            &i2c_bus_config,
            &bus_handle
        ),
        TAG,
        "Could not initialize new master bus"
    );

    return ESP_OK;
}
void app_main(void) {

    // We will use different event loops for each logic task following isolation principles

    // An event loop for sensoring related events

    esp_event_loop_args_t sgp30_event_loop_args = {
        .queue_size = 5,
        .task_name = "sgp30_event_loop_task", /* since it is a task it can be stopped */
        .task_stack_size = 4096,
        .task_priority = uxTaskPriorityGet(NULL),
        .task_core_id = tskNO_AFFINITY,
    };
    esp_event_loop_create(&sgp30_event_loop_args, &sgp30_event_loop_handle);

    // An event loop for storage related events

    esp_event_loop_args_t storage_event_loop_args = {
        .queue_size = 5,
        .task_name = "storage_event_loop_task", /* since it is a task it can be stopped */
        .task_stack_size = 4096,
        .task_priority = uxTaskPriorityGet(NULL),
        .task_core_id = tskNO_AFFINITY,
    };

    esp_event_loop_create(&storage_event_loop_args, &storage_event_loop_handle);

    // An event loop for communication related events

    esp_event_loop_args_t communication_event_loop_args = {
        .queue_size = 5,
        .task_name = "communication_event_loop_task", /* since it is a task it can be stopped */
        .task_stack_size = 4096,
        .task_priority = uxTaskPriorityGet(NULL),
        .task_core_id = tskNO_AFFINITY,
    };

    esp_event_loop_create(&communication_event_loop_args, &communication_event_loop_handle);

    // We initiate the NVS module

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition was truncated
         * and needs to be erased */
        ESP_ERROR_CHECK(nvs_flash_erase());

        /* Retry nvs_flash_init */
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(ret);

    // Set up communication for NVS module.
    // QueueHandle_t storage_sgp30_baseline_queue = xQueueCreate(1, sizeof(sgp30_log_entry_t));


    // ESP_ERROR_CHECK(
    //     esp_event_handler_register_with(
    //         storage_event_loop_handle,
    //         STORAGE_EVENT,
    //         STORAGE_COMMAND_RETURN_GET_BASELINE,
    //         storage_command_return_get_baseline_event_handler,
    //         &sgp30_baseline_queue
    //     )
    // );
    // 
    // [TODO]

    // Initiate sgp30 module

    ESP_ERROR_CHECK(init_i2c());
    ESP_ERROR_CHECK(sgp30_device_create(bus_handle, SGP30_I2C_ADDR, 400000));


    // Set up event listeners for SGP30 module.

    ESP_ERROR_CHECK(
        esp_event_handler_register_with(
            sgp30_event_loop_handle,
            SENSOR_EVENTS,
            SENSOR_EVENT_IAQ_INITIALIZED,
            sgp30_on_iaq_initialized,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_register_with(
            sgp30_event_loop_handle,
            SENSOR_EVENTS,
            SENSOR_EVENT_NEW_MEASUREMENT,
            sgp30_on_new_measurement,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_register_with(
            sgp30_event_loop_handle,
            SENSOR_EVENTS,
            SENSOR_EVENT_NEW_BASELINE,
            sgp30_on_new_baseline,
            NULL
        )
    );
    // Obtain baseline from NVS if available
    // sgp30_log_entry_t sgp30_last_stored_baseline;
    // ESP_ERROR_CHECK(
    //     storage_invoke_get_baseline_command(
    //         storage_sgp30_baseline_queue,
    //         &sgp30_last_stored_baseline
    //     )
    // );
    // sgp30_log_entry_to_valid_baseline_or_null(&sgp30_last_stored_baseline, &sgp30_baseline);
    // Init SGP30 sensor using obtained baseline
    sgp30_init(sgp30_event_loop_handle, NULL);
       /* WIFI Y SNTP
     // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "NVS initialized successfully");

    // Initialize Wi-Fi
    ESP_LOGI(TAG, "Initializing Wi-Fi...");
    wifi_init_sta();

    // Wait for Wi-Fi connection
    ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
    int retries = 0;
    while (retries < 20) {
        if (esp_wifi_connect() == ESP_OK) {
            ESP_LOGI(TAG, "Wi-Fi connected successfully!");
            break;
        }
        retries++;
        ESP_LOGI(TAG, "Retrying Wi-Fi connection... Attempt %d", retries);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    if (retries == 20) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi after multiple attempts");
        return;  // Stop execution if Wi-Fi connection fails
    }

    // Initialize SNTP for time synchronization
    ESP_LOGI(TAG, "Starting SNTP synchronization...");
    initialize_sntp();

    // Continue with the rest of the application logic
    */
}
