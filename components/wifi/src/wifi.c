#include "wifi.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"


static const char *TAG = "WIFI";

// WiFi configuration (using WPA2)
wifi_config_t wifi_config = {
    .sta = {
        .ssid = CONFIG_ESP_WIFI_SSID,
        .password = CONFIG_ESP_WIFI_PASSWORD,
        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        .pmf_cfg = {
            .capable = true,
            .required = false
        },
    },
};

// wifi_event_handler: Aquí es donde manejas la reconexión WiFi
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


void wifi_init_sta(void) {
    // Initialize WiFi stack and set the WiFi mode to STA (Station mode)
    ESP_ERROR_CHECK(esp_netif_init());  // Initialize the network interface layer
    ESP_ERROR_CHECK(esp_event_loop_create_default());  // Create the default event loop

    esp_netif_create_default_wifi_sta();  // Create default WiFi interface for STA mode

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));  // Initialize the WiFi driver

    // Register event handler to listen to WiFi events (e.g., connected, disconnected)
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));  // Set WiFi mode to STA
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));  // Set WiFi configuration (SSID, password)

    // Start the WiFi driver
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi initialization completed");
}
