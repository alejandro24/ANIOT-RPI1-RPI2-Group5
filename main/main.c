#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "mqtt_controller.h"
#include "nvs_structures.h"
#include "sgp30.h"
#include "sgp30_types.h"
#include "softAP_provision.h"
#include "thingsboard_types.h"
#include <esp_wifi.h>
#include <string.h>

#include "esp_log.h"

#define DEVICE_SDA_IO_NUM 21
#define DEVICE_SCL_IO_NUM 22

static char *TAG = "MAIN";
// sgp30 required structures
i2c_master_bus_handle_t i2c_master_bus_handle;
esp_event_loop_handle_t imc_event_loop_handle;
SemaphoreHandle_t sgp30_req_measurement;
sgp30_measurement_log_t sgp30_log;
uint16_t send_time = 30;
thingsboard_cfg_t thingsboard_cfg;
wifi_credentials_t wifi_credentials;

// wifi handler to take actions for the different wifi events
static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    static int retry_count = 0;

    switch (event_id)
    {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "Conectando a WiFi...");
            esp_wifi_connect(); // Intenta la conexión
            break;
        case WIFI_EVENT_STA_CONNECTED:
            retry_count = 0; // Reseteamos el contador de reintentos
            ESP_LOGI(TAG, "Conectado exitosamente al WiFi");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Desconectado del WiFi, reintentando...");
            if (retry_count < 5)
            {
                int delay = (1 << retry_count)
                            * 1000; // Backoff exponencial (1s, 2s, 4s...)
                ESP_LOGI(TAG, "Reintentando en %d ms...", delay);
                vTaskDelay(pdMS_TO_TICKS(delay)
                ); // Esperar antes de intentar nuevamente
                esp_wifi_connect();
                retry_count++;
            } else
            {
                ESP_LOGE(
                    TAG,
                    "No se pudo conectar al WiFi después de varios "
                    "intentos."
                );
                // Acciones adicionales si fallan todos los intentos
            }
            break;
        default:
            break;
    }
}

static void event_handler_got_ip(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));

    mqtt_init(imc_event_loop_handle, &thingsboard_cfg);
}

static void sgp30_on_new_measurement(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
)
{
    sgp30_timed_measurement_t new_log_entry;
    time(&new_log_entry.time);

    new_log_entry.measurement = *((sgp30_measurement_t *)event_data);
    ESP_LOGI(
        TAG,
        "Measured eCO2= %d TVOC= %d",
        new_log_entry.measurement.eCO2,
        new_log_entry.measurement.TVOC
    );
    // sgp30_measurement_enqueue(&new_log_entry, &sgp30_log);
    // char[400] measurement_JSON_repr;
    // sgp30_measurement_to_JSON(&new_log_entry.measurement,
    // &measurement_JSON_rep) mqtt_publish(measurement_JSON_repr);
    //  Send or store log_entry
}

static void mqtt_on_new_interval(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
)
{
    uint32_t new_measurement_interval = *((uint32_t *)event_data);
    sgp30_restart_measuring(new_measurement_interval);
}

static void sgp30_on_new_baseline(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
)
{
    sgp30_timed_measurement_t new_baseline;
    new_baseline.measurement = *((sgp30_measurement_t *)event_data),
    time(&new_baseline.time);
    ESP_ERROR_CHECK(storage_set((const sgp30_timed_measurement_t*) &new_baseline));
    ESP_LOGI(
        TAG,
        "Baseline eCO2= %d TVOC= %d at timestamp %s",
        new_baseline.measurement.eCO2,
        new_baseline.measurement.TVOC,
        ctime(&new_baseline.time)
    );
}

static const sgp30_event_handler_register_t sgp30_registered_events[] = {
    { SGP30_EVENT_NEW_MEASUREMENT, sgp30_on_new_measurement },
    { SGP30_EVENT_NEW_MEASUREMENT,    sgp30_on_new_baseline }
};

static const size_t sgp30_registered_events_len =
    sizeof(sgp30_registered_events) / sizeof(sgp30_registered_events[0]);

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
        i2c_new_master_bus(&i2c_bus_config, bus_handle),
        TAG,
        "Could not initialize new master bus"
    );

    return ESP_OK;
}

char *prepare_meassure_send(long ts, sgp30_measurement_t measurement)
{
    cJSON *json_data = cJSON_CreateObject();
    cJSON *measurement_json = cJSON_CreateObject();
    char *data_to_send;
    cJSON_AddNumberToObject(json_data, "ts", ts);
    cJSON_AddNumberToObject(measurement_json, "eCO2", measurement.eCO2);
    cJSON_AddNumberToObject(measurement_json, "TVOC", measurement.TVOC);
    cJSON_AddItemToObjectCS(json_data, "values", measurement_json);
    data_to_send = cJSON_Print(json_data);

    cJSON_Delete(json_data);
    cJSON_Delete(measurement_json);
    return data_to_send;
}

void app_main(void)
{

    esp_event_loop_args_t imc_event_loop_args = {
        // An event loop for sensoring related events
        .queue_size = 5,
        .task_name =
            "sgp30_event_loop_task", /* since it is a task it can be stopped */
        .task_stack_size = 4096,
        .task_priority = uxTaskPriorityGet(NULL),
        .task_core_id = tskNO_AFFINITY,
    };
    esp_event_loop_create(&imc_event_loop_args, &imc_event_loop_handle);

    ESP_ERROR_CHECK(storage_init());

    ESP_ERROR_CHECK(init_i2c(&i2c_master_bus_handle));
    ESP_ERROR_CHECK(
        sgp30_device_create(i2c_master_bus_handle, SGP30_I2C_ADDR, 400000)
    );

    // Set up event listeners for SGP30 module.
    for (int i = 0; i < sgp30_registered_events_len; i++)
    {
        ESP_ERROR_CHECK(
            esp_event_handler_register_with(
                imc_event_loop_handle,
                SGP30_EVENT,
                sgp30_registered_events[i].event_id,
                sgp30_registered_events[i].event_handler,
                NULL
            )
        );
    }


    // Set up event listeenr for MQTT module
    ESP_ERROR_CHECK(
        esp_event_handler_register_with(
            imc_event_loop_handle,
            MQTT_THINGSBOARD_EVENT,
            MQTT_NEW_SEND_TIME,
            mqtt_on_new_interval,
            NULL
        )
    );

    esp_err_t got_thingboard_cfg = storage_get(&thingsboard_cfg);
    esp_err_t got_wifi_credentials = storage_get(&wifi_credentials);
    if( got_thingboard_cfg != got_wifi_credentials)
    {
        ESP_LOGE(TAG, "Provisioning State Corrupt");
        //Start the init of the provision component, we actively wait it to finish the provision to continue
        ESP_ERROR_CHECK(softAP_provision_init(NULL, NULL));
        thingsboard_cfg = get_thingsboard_cfg();
        wifi_credentials = get_wifi_credentials();
        storage_set((const thingsboard_cfg_t*) &thingsboard_cfg);
        storage_set((const wifi_credentials_t*) &wifi_credentials);
    }
    else if(got_thingboard_cfg != ESP_OK)
    {
        ESP_LOGI(TAG, "Device not provisioned");
        //Start the init of the provision component, we actively wait it to finish the provision to continue
        ESP_ERROR_CHECK(softAP_provision_init(NULL, NULL));
        thingsboard_cfg = get_thingsboard_cfg();
        wifi_credentials = get_wifi_credentials();
        storage_set((const thingsboard_cfg_t*)&thingsboard_cfg);
        storage_set((const wifi_credentials_t*)&wifi_credentials);
    }
    else
    {
        ESP_LOGI(TAG, "Device provisioned");
        ESP_ERROR_CHECK(softAP_provision_init(&thingsboard_cfg, &wifi_credentials));
    }

    // At this point a valid time is required
    // We start the sensor
    sgp30_timed_measurement_t maybe_baseline;
    time_t time_now;
    time(&time_now);
    if (ESP_OK == storage_get(&maybe_baseline)
        && !sgp30_is_baseline_expired(maybe_baseline.time, time_now))
    {
        sgp30_init(imc_event_loop_handle, &maybe_baseline.measurement);
    } else
    {
        sgp30_init(imc_event_loop_handle, NULL);
    }
    // Esto debería de iniciarse al tener un valor del intervalo, por MQTT
    // (atributo compartido creo) Se inicia solo al mandar un evento
    // SGP30_EVENT_NEW_INTERVAL
    mqtt_init(imc_event_loop_handle, &thingsboard_cfg);

    sgp30_start_measuring(1);

    /* WIFI Y SNTP
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  { ESP_ERROR_CHECK(nvs_flash_erase()); ret = nvs_flash_init();
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


  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
     // NVS partition was truncated and needs to be erased

     ESP_ERROR_CHECK(nvs_flash_erase());

     //Retry nvs_flash_init
     ESP_ERROR_CHECK(nvs_flash_init());
  }

  ESP_ERROR_CHECK(ret);
  */
}
