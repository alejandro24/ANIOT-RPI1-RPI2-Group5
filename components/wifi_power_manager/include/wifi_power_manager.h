#ifndef WIFI_POWER_MANAGER_H
#define WIFI_POWER_MANAGER_H

#include "esp_err.h"

/* Enum for Wi-Fi power modes*/
typedef enum {
    WIFI_POWER_MODE_MIN_MODEM,
    WIFI_POWER_MODE_MAX_MODEM,
    WIFI_POWER_MODE_NONE
} wifi_power_mode_t;

/* Function to initialize the Wi-Fi power manager*/
esp_err_t wifi_power_save_init(void);

/* Function to set the Wi-Fi power mode*/
esp_err_t wifi_set_power_mode(wifi_power_mode_t mode);

#endif /* WIFI_POWER_MANAGER_H*/
