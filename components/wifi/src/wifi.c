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
        .ssid = CONFIG_ESP_WIFI_SSID,         // WiFi SSID from configuration
        .password = CONFIG_ESP_WIFI_PASSWORD, // WiFi password from configuration
        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        .pmf_cfg = {
            .capable = true,
            .required = false
        },
    },
};

// Wi-Fi event handler to manage connection/reconnection
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    static int retry_count = 0;

    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "Attempting to connect to WiFi...");
            esp_wifi_connect();  // Start the connection attempt
            break;
        case WIFI_EVENT_STA_CONNECTED:
            retry_count = 0;  // Reset retry counter on successful connection
            ESP_LOGI(TAG, "Successfully connected to WiFi");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected from WiFi, retrying...");
            if (retry_count < 5) {
                int delay = (1 << retry_count) * 1000;  // Exponential backoff (1s, 2s, 4s...)
                ESP_LOGI(TAG, "Retrying in %d ms...", delay);
                vTaskDelay(pdMS_TO_TICKS(delay));  // Wait before retrying connection
                esp_wifi_connect();
                retry_count++;
            } else {
                ESP_LOGE(TAG, "Failed to connect to WiFi after several attempts.");
                // Handle further failure actions if needed (e.g., reset, alert, etc.)
            }
            break;
        default:
            break;
    }
}

// Function to initialize Wi-Fi in Station mode
void wifi_init_sta(void) {
    ESP_LOGI(TAG, "Initializing WiFi...");

    // Initialize the network interface layer
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default WiFi interface in Station mode
    esp_netif_create_default_wifi_sta();

    // Set up WiFi configuration (using WPA2)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));  // Initialize the Wi-Fi driver

    // Register the event handler to manage Wi-Fi events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    // Set Wi-Fi mode to STA (Station mode)
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Set the Wi-Fi configuration (SSID, password)
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Start the Wi-Fi driver
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi initialization completed");
}
