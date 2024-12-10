#ifndef SNTP_SYNC_H
#define SNTP_SYNC_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "time.h"

// Wi-Fi credentials
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"

// NTP Server configuration
#define NTP_SERVER "pool.ntp.org"

// Function declarations
void wifi_init();
void sntp_init();
bool time_is_set();
bool get_time(struct tm *timeinfo);

#endif // SNTP_SYNC_H
