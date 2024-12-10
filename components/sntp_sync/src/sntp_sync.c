#include "sntp_sync.h"

static const char *TAG = "SNTP_SYNC";

// Wi-Fi event handler
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                ESP_LOGI(TAG, "WiFi started");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "WiFi disconnected, trying to reconnect...");
                esp_wifi_connect();
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                ESP_LOGI(TAG, "Got IP: %s", ip4addr_ntoa(&event->ip_info.ip));
                break;
            }
        }
    }
}
// Initialize Wi-Fi
void wifi_init() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
}

// Initialize SNTP
void my_sntp_init() {  // Rename to avoid conflict
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setserver(0, (const char *)NTP_SERVER);
}

// Check if time has been set
bool time_is_set() {
    struct tm timeinfo;
    if (get_time(&timeinfo)) {
        return true;
    }
    return false;
}

// Get current time
bool get_time(struct tm *timeinfo) {
    time_t now;
    struct tm timeinfo_tmp;

    time(&now);
    localtime_r(&now, &timeinfo_tmp);

    if (timeinfo_tmp.tm_year > (2016 - 1900)) {
        *timeinfo = timeinfo_tmp;
        return true;
    }
    return false;
}
