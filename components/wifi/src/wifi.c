#include "wifi.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "WIFI";

// WiFi configuration (using WPA2)
wifi_config_t wifi_config = {
    .sta = {
        .ssid = CONFIG_ESP_WIFI_SSID,  // Defined in menuconfig
        .password = CONFIG_ESP_WIFI_PASSWORD,  // Defined in menuconfig
        .authmode = WIFI_AUTH_WPA2_PSK,
        .pmf_cfg = { 
            .required = false, 
        },
    },
};

// WiFi event handler to manage connection and reconnection
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    static int retry_count = 0;
    
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "Connecting to WiFi...");
            esp_wifi_connect();  // Initiates WiFi connection
            break;
        case WIFI_EVENT_STA_CONNECTED:
            retry_count = 0;  // Reset retry count upon successful connection
            ESP_LOGI(TAG, "Successfully connected to WiFi");

            // Enable Power Save Mode after connection
            ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));  // Set power save mode to minimum
            ESP_LOGI(TAG, "WiFi Power Save Mode enabled");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WiFi disconnected, attempting reconnection...");
            if (retry_count < 5) {  // Try reconnecting up to 5 times
                esp_wifi_connect();
                retry_count++;
            } else {
                ESP_LOGE(TAG, "Failed to connect to WiFi after 5 attempts");
                // Here, you can trigger some other action if the connection fails (e.g., LED blink, reset, etc.)
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
