#include <stdio.h> 
#include <string.h>
#include "esp_log.h"
#include "wifi.h"  // Include your WiFi functions
#include "esp_system.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sntp.h"  // Use esp_sntp.h for SNTP functionality
#include "lwip/dns.h"  // For DNS resolution
#include "lwip/netif.h"  // For network interface handling

static const char *TAG = "MAIN";

// Global flag to indicate if SNTP sync is complete
static bool sntp_synced = false;

// SNTP callback function to notify when sync is complete
static void sntp_sync_callback(void) {
    sntp_synced = true;  // Set flag when SNTP sync is done
}

// Set DNS server (Google DNS)
void set_dns(void) {
    ip4_addr_t dns_server;
    IP4_ADDR(&dns_server, 8, 8, 8, 8);  // Google's public DNS server

    // Set primary DNS
    dns_setserver(0, &dns_server);

    // Optionally, set secondary DNS (e.g., 8.8.4.4)
    IP4_ADDR(&dns_server, 8, 8, 4, 4);
    dns_setserver(1, &dns_server);

    ESP_LOGI(TAG, "DNS servers set.");
}

// Initialize SNTP
void initialize_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP...");

    // Set SNTP operating mode to polling
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);  // Use polling mode

    // Resolve NTP server address using DNS
    struct ip_addr ntp_server_ip;
    esp_err_t err = dns_gethostbyname("pool.ntp.org", &ntp_server_ip, NULL, NULL);
    if (err != ERR_OK) {
        ESP_LOGE(TAG, "Failed to resolve NTP server address: %s", esp_err_to_name(err));
        return;
    }

    // Set NTP server
    esp_sntp_setserver(0, &ntp_server_ip);  // Set NTP server (index 0)

    // Initialize SNTP
    esp_sntp_init();

    // Polling SNTP synchronization status
    ESP_LOGI(TAG, "Waiting for SNTP sync...");
    int retries = 0;
    while (!sntp_synced && retries < 10) {  // Wait for sync flag
        ESP_LOGI(TAG, "Retrying SNTP sync... Attempt %d", retries + 1);
        vTaskDelay(pdMS_TO_TICKS(1000));  // Wait 1 second before retrying
        retries++;
    }

    if (sntp_synced) {
        ESP_LOGI(TAG, "SNTP sync successful!");
    } else {
        ESP_LOGE(TAG, "SNTP sync failed after multiple attempts.");
    }
}

void app_main(void) {
    // Initialize NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());  // If NVS is corrupted, erase and reinitialize
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);  // Ensure NVS initialization is successful

    ESP_LOGI(TAG, "NVS initialized successfully");

    // Initialize Wi-Fi
    ESP_LOGI(TAG, "Initializing Wi-Fi...");
    wifi_init_sta();  // Initialize Wi-Fi (from your wifi.c file)

    // Wait for Wi-Fi connection
    ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
    int retries = 0;
    while (retries < 20) {  // Set retry limit
        if (esp_wifi_connect() == ESP_OK) {
            ESP_LOGI(TAG, "Wi-Fi connected successfully!");
            break;
        }
        retries++;
        ESP_LOGI(TAG, "Retrying Wi-Fi connection... Attempt %d", retries);
        vTaskDelay(pdMS_TO_TICKS(2000));  // Delay for 2 seconds before retrying
    }

    if (retries == 20) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi after multiple attempts");
        return;  // Exit if Wi-Fi connection fails
    }

    // Set DNS after Wi-Fi connection
    set_dns();

    // Initialize SNTP after Wi-Fi is connected
    initialize_sntp();

    // Continue with the rest of your application logic here
    // For example, start sensor monitoring, etc.
}
