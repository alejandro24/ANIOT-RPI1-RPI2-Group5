#ifndef WIFI_H
#define WIFI_H

#include <esp_wifi.h>

// Wi-Fi configuration (using WPA2)
extern wifi_config_t wifi_config;  // Declare wifi_config, defined in wifi.c

// Initializes WiFi in Station mode (STA)
void wifi_init_sta(void);

// Debugging utility: Logs the current Power Save mode (optional)
void wifi_get_ps_mode(void);

// Optional: Configures the maximum retry attempts for WiFi reconnection
void set_wifi_retry_limit(int retries);

#endif // WIFI_H
