//#include "wifi_manager.h"
#include "driver/i2c_types.h"
#include "esp_log.h"
#include "wifi.h"

#include "esp_event.h"
#include "esp_event_base.h"
#include "sgp30.h"
#include <stdint.h>
#include <string.h>

static char* TAG = "MAIN";
i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t sgp30;
esp_event_loop_handle_t sgp30_event_loop_handle;

static void sgp30_event_handler(
    void * handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data)
{
    ESP_LOGD(TAG, "Event dispached from event loop base=%s, event_id=%" PRIi32
             "", base, event_id);
    sgp30_measurement_t new_measurement;
    sgp30_baseline_t new_baseline;
    switch ((mox_event_id_t) event_id) {
        case SENSOR_EVENT_NEW_MEASUREMENT:
            new_measurement = *((sgp30_measurement_t*) event_data);
            ESP_LOGI(TAG, "Measured eCO2= %d TVOC= %d", new_measurement.eCO2, new_measurement.TVOC);
            break;

        case SENSOR_IAQ_INITIALIZING:
            ESP_LOGI(TAG, "SGP30 Initializing ...");
            break;

        case SENSOR_IAQ_INITIALIZED:
            ESP_LOGI(TAG, "SGP30 Initialized.");
            break;
        case SENSOR_GOT_BASELINE:
            new_baseline = *((sgp30_baseline_t*) event_data);
            ESP_LOGI(TAG, "Baseline eCO2= %d TVOC= %d at timestamp %" PRIi64 "", new_baseline.baseline.eCO2, new_baseline.baseline.TVOC, new_baseline.timestamp);

        default:
            ESP_LOGD(TAG, "Unhandled event_id");
            break;
    }
}

void init_i2c(void)
{
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = 22,
        .sda_io_num = 21,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    ESP_ERROR_CHECK(sgp30_device_create(bus_handle, SGP30_I2C_ADDR, 400000));
}
void app_main(void) {

    init_i2c();

    esp_event_loop_args_t sgp30_event_loop_args = {
        .queue_size =5,
        .task_name = "sgp30_event_loop_task", /* since it is a task it can be stopped */
        .task_stack_size = 4096,
        .task_priority = uxTaskPriorityGet(NULL),
        .task_core_id = tskNO_AFFINITY,
    };

    esp_event_loop_create(&sgp30_event_loop_args, &sgp30_event_loop_handle);
    ESP_ERROR_CHECK(
        esp_event_handler_register_with(
            sgp30_event_loop_handle,
            SENSOR_EVENTS,
            ESP_EVENT_ANY_ID,
            sgp30_event_handler,
            NULL
        )
    );

    sgp30_init(sgp30_event_loop_handle);
    // Inicializa la gestión Wi-Fi
    //wifi_manager_init();

    // Conectar al Wi-Fi con los parámetros deseados
    
    
     ESP_LOGI(TAG, "Initializing WiFi...");
    wifi_init_sta();  // Call your WiFi initialization function

 //wifi_manager_connect("MiFibra-F4A8", "7tcHKtYk!");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // Wait a second to avoid a busy loop
}
}