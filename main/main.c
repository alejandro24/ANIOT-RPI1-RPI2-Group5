//#include "wifi_manager.h"
#include "driver/i2c_types.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_timer.h"
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


static char* TAG = "MAIN";
// sgp30 required structures
i2c_master_bus_handle_t i2c_master_bus_handle;
esp_event_loop_handle_t imc_event_loop_handle;
uint32_t sgp30_req_measurement_interval;
esp_timer_handle_t sgp30_req_measurement_timer_handle;
SemaphoreHandle_t sgp30_req_measurement;
sgp30_log_t sgp30_log;

static void sgp30_req_measurement_callback(void *args) {
    sgp30_request_measurement();
}

static void sgp30_on_new_measurement(
    void * handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    sgp30_log_entry_t new_log_entry;
    time(&new_log_entry.tv);
    new_log_entry.measurements = *((sgp30_measurement_t*) event_data);
    ESP_LOGI(TAG, "Measured eCO2= %d TVOC= %d", new_log_entry.measurements.eCO2, new_log_entry.measurements.TVOC);
    // sgp30_measurement_enqueue(&new_log_entry, &sgp30_log);
    // Send or store log_entry
}

static void sgp30_on_new_interval(
    void * handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    uint32_t new_measurement_interval = *((uint32_t*) event_data);
    if (esp_timer_is_active(sgp30_req_measurement_timer_handle)) {
        esp_timer_restart(sgp30_req_measurement_timer_handle, new_measurement_interval);
    } else {
        esp_timer_start_periodic(sgp30_req_measurement_timer_handle, new_measurement_interval);
    }
}

static void sgp30_on_new_baseline(
    void * handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    sgp30_log_entry_t new_baseline;
    new_baseline.measurements = *((sgp30_measurement_t*) event_data),
    time(&new_baseline.tv);
    //storage_set
    ESP_LOGI(
        TAG,
        "Baseline eCO2= %d TVOC= %d at timestamp %s",
        new_baseline.measurements.eCO2,
        new_baseline.measurements.TVOC,
        ctime(&new_baseline.tv)
    );
}

static esp_err_t init_i2c(i2c_master_bus_handle_t *bus_handle)
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
            bus_handle
        ),
        TAG,
        "Could not initialize new master bus"
    );

    return ESP_OK;
}

void app_main(void) {


    esp_event_loop_args_t imc_event_loop_args = {
        .queue_size = 5,
        .task_name = "sgp30_event_loop_task", /* since it is a task it can be stopped */
        .task_stack_size = 4096,
        .task_priority = uxTaskPriorityGet(NULL),
        .task_core_id = tskNO_AFFINITY,
    };
    esp_event_loop_create(&imc_event_loop_args, &imc_event_loop_handle);


    ESP_ERROR_CHECK(storage_init());


    ESP_ERROR_CHECK(init_i2c(&i2c_master_bus_handle));
    ESP_ERROR_CHECK(sgp30_device_create(i2c_master_bus_handle, SGP30_I2C_ADDR, 400000));


    // Set up event listeners for SGP30 module.
    ESP_ERROR_CHECK(
        esp_event_handler_register_with(
            imc_event_loop_handle,
            SGP30_EVENT,
            SGP30_EVENT_NEW_INTERVAL,
            sgp30_on_new_interval,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_register_with(
            imc_event_loop_handle,
            SGP30_EVENT,
            SGP30_EVENT_NEW_MEASUREMENT,
            sgp30_on_new_measurement,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_register_with(
            imc_event_loop_handle,
            SGP30_EVENT,
            SGP30_EVENT_NEW_BASELINE,
            sgp30_on_new_baseline,
            NULL
        )
    );

    esp_timer_create_args_t sgp30_req_measurement_timer_args = {
        .callback = sgp30_req_measurement_callback,
        .name = "request_measurement"
    };

    esp_timer_create(&sgp30_req_measurement_timer_args, &sgp30_req_measurement_timer_handle);
    // At this point a valid time is required
    // We start the sensor
    sgp30_log_entry_t maybe_baseline;
    time_t time_now;
    time(&time_now);
    if ( ESP_OK == storage_get(&maybe_baseline) && !sgp30_is_baseline_expired(maybe_baseline.tv, time_now)){
        sgp30_init(imc_event_loop_handle, &maybe_baseline.measurements);
    } else {
        sgp30_init(imc_event_loop_handle, NULL);
    }
    // Esto debería de iniciarse al tener un valor del intervalo, por MQTT (atributo compartido creo)
    // Se inicia solo al mandar un evento SGP30_EVENT_NEW_INTERVAL
    esp_timer_start_periodic(sgp30_req_measurement_timer_handle, 10000000);

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
/   */
}
