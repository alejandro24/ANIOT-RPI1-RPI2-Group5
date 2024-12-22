#include <esp_wifi.h>
#include "driver/i2c_types.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "sgp30.h"
#include "softAP_provision.h"
#include <stdint.h>
#include <string.h>
#include <nvs_flash.h>

static char* TAG = "MAIN";
i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t sgp30;
esp_event_loop_handle_t sgp30_event_loop_handle;
static EventGroupHandle_t provision_event_group;

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
    time_t now;
    time(&now);
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
             sgp30_start_measuring();
             break;

        case SENSOR_GOT_BASELINE:
             new_baseline = *((sgp30_baseline_t*) event_data);
             ESP_LOGI(
                 TAG,
                 "Baseline eCO2= %d TVOC= %d at timestamp %s",
                 new_baseline.baseline.eCO2,
                 new_baseline.baseline.TVOC,
                 ctime(&now)
             );
             break;

        default:
            ESP_LOGD(TAG, "Unhandled event_id");
            break;
    }
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
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
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

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition was truncated
         * and needs to be erased */
        ESP_ERROR_CHECK(nvs_flash_erase());

        /* Retry nvs_flash_init */
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(ret);

    init_i2c();

    /* Initialize the event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler_got_ip, NULL));

    provision_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(softAP_provision_init(provision_event_group));

    /* Wait for Provision*/
    xEventGroupWaitBits(provision_event_group, PROVISION_DONE_EVENT, true, true, portMAX_DELAY);

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
}
