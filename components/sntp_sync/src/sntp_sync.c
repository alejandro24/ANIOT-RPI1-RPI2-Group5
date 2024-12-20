#include <stdio.h>
#include <time.h>  // For system time functions
#include "esp_log.h"
#include "lwip/apps/sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SNTP_SYNC";

void initialize_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP...");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
   sntp_setservername(0, "time.google.com");
  // Use default NTP server
    sntp_init();

    // Wait for SNTP time synchronization
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2020 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));  // Delay 2 seconds
        time(&now);  // Get current time
        localtime_r(&now, &timeinfo);
    }

    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGE(TAG, "Failed to get time from SNTP server");
    } else {
        ESP_LOGI(TAG, "Time synchronized successfully: %s", asctime(&timeinfo));
    }
}
