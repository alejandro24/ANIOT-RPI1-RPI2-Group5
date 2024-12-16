#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sntp_sync.h"  // Include the SNTP component

static const char *TAG = "MAIN";

void app_main(void) {
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
}
