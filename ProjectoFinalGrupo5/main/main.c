#include <stdio.h>
#include "esp_log.h"
#include "wifi.h"  // Include the WiFi management functions
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";

// monitorea el estado del wifi
void wifi_monitor_task(void *pvParameters) {
    while (1) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "conectado a SSID: %s, RSSI: %d dBm", ap_info.ssid, ap_info.rssi);
        } else {
            ESP_LOGW(TAG, "no conectado a wifi");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));  // Cada 5 segundos
    }
}

void app_main(void) {
    // ********************** WiFi Initializer ****************************
    //ni puta idea si esto funciona
    ESP_LOGI(TAG, "iniciando wifi");
    wifi_init_sta();  

    // Start a task to monitor WiFi status
    xTaskCreate(wifi_monitor_task, "monitor", 2048, NULL, 5, NULL);

    while (1) {
        ESP_LOGI(TAG, ".........");
        vTaskDelay(pdMS_TO_TICKS(1000));  // delay
    }
    // *********************************************************************
}
