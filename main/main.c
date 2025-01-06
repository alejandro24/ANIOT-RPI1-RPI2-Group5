#include <esp_wifi.h>
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
#include "softAP_provision.h"
#include <stdint.h>
#include <string.h>
#include <nvs_flash.h>
#include "sntp_sync.h"  // Include the SNTP component
#include "wifi.h"
#include "mqtt.h"
#include <stdio.h>


#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/apps/sntp.h"

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
esp_event_loop_handle_t mqtt_thingsboard_event_loop_handle;
static EventGroupHandle_t provision_event_group;
char thingsboard_url[100]; 
wifi_credentials_t *wifi_credentials;

static void new_send_time_event_handler(
    void * handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data){
        send_time = *(int*) event_data;
    }

// wifi handler to take actions for the different wifi events
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    static int retry_count = 0;
    
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "Conectando a WiFi...");
            esp_wifi_connect();  // Intenta la conexión
            break;
        case WIFI_EVENT_STA_CONNECTED:
            retry_count = 0;  // Reseteamos el contador de reintentos
            ESP_LOGI(TAG, "Conectado exitosamente al WiFi");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Desconectado del WiFi, reintentando...");
            if (retry_count < 5) {
                int delay = (1 << retry_count) * 1000;  // Backoff exponencial (1s, 2s, 4s...)
                ESP_LOGI(TAG, "Reintentando en %d ms...", delay);
                vTaskDelay(pdMS_TO_TICKS(delay));  // Esperar antes de intentar nuevamente
                esp_wifi_connect();
                retry_count++;
            } else {
                ESP_LOGE(TAG, "No se pudo conectar al WiFi después de varios intentos.");
                // Acciones adicionales si fallan todos los intentos
            }
            break;
        default:
            break;
    }
}

static void event_handler_got_ip(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    esp_event_loop_handle_t default_loop = esp_event_loop_get_default();
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    mqtt_init(thingsboard_url, default_loop);
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
    esp_event_loop_handle_t default_loop = esp_event_loop_get_default();
    
    esp_event_loop_args_t sgp30_event_loop_args = {
        .queue_size = 5,
        .task_name = "sgp30_event_loop_task", /* since it is a task it can be stopped */
        .task_stack_size = 4096,
        .task_priority = uxTaskPriorityGet(NULL),
        .task_core_id = tskNO_AFFINITY,
    };
    esp_event_loop_create(&sgp30_event_loop_args, &sgp30_event_loop_handle);

    // We initiate the NVS module
    esp_err_t ret = nvs_flash_init();

    ESP_ERROR_CHECK(init_i2c());
    ESP_ERROR_CHECK(sgp30_device_create(bus_handle, SGP30_I2C_ADDR, 400000));


    // Set up event listeners for SGP30 module.
    ESP_ERROR_CHECK(
        esp_event_handler_register_with(
            sgp30_event_loop_handle,
            SGP30_EVENT,
            SGP30_EVENT_NEW_INTERVAL,
            sgp30_on_new_interval,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_register_with(
            sgp30_event_loop_handle,
            SGP30_EVENT,
            SGP30_EVENT_NEW_MEASUREMENT,
            sgp30_on_new_measurement,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_register_with(
            sgp30_event_loop_handle,
            SGP30_EVENT,
            SGP30_EVENT_NEW_BASELINE,
            sgp30_on_new_baseline,
            NULL
        )
    );

        ESP_ERROR_CHECK(
        esp_event_handler_register_with(
            ,
            MQTT_THINGSBOARD_EVENTS,
            MQTT_NEW_SEND_TIME,
            mqtt_event_handler,
            NULL
        )
    );

    //TODO: Intentar sacar de nvs los datos de provisionamiento para pasarlos a el init de provisionamiento
    wifi_credentials = (wifi_credentials_t*)(sizeof(wifi_credentials_t));
    //Start the init of the provision component, we actively wait it to finish the provision to continue            
    provision_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(softAP_provision_init(provision_event_group, thingsboard_url, wifi_credentials));

    /* Wait for Provision*/
    xEventGroupWaitBits(provision_event_group, PROVISION_DONE_EVENT, true, true, portMAX_DELAY);
    
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
        return; // Stop execution if Wi-Fi connection fails
    }

    // Attempt SNTP synchronization
    if (!obtain_time()) {
        ESP_LOGE(TAG, "SNTP synchronization failed after multiple attempts");
        return; // Stop execution if SNTP synchronization fails
    }

    // Continue with the rest of the application logic
    ESP_LOGI(TAG, "Application logic continues...");

    
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased 
         
        ESP_ERROR_CHECK(nvs_flash_erase());

        //Retry nvs_flash_init 
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(ret);
*/
}
