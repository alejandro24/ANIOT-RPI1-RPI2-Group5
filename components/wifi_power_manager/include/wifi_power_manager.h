#ifndef WIFI_POWER_MANAGER_H
#define WIFI_POWER_MANAGER_H

#include "esp_err.h"

/* Enum for Wi-Fi power modes*/
typedef enum {
    WIFI_POWER_MODE_MIN_MODEM,
    WIFI_POWER_MODE_MAX_MODEM,
    WIFI_POWER_MODE_NONE
} wifi_power_mode_t;

/**
 * @brief Function to initialize the Wi-Fi power manager. 
 *
 * @param 
 * @return esp_err_t
 *
 */

esp_err_t wifi_power_save_init(void);

/**
 * @brief This function configures the Wi-Fi power mode based on the value of mode, logging the configuration made. 
 * If the power mode is invalid, it logs an error and returns ESP_ERR_INVALID_ARG.. 
 *
 * @param wifi_power_mode_t mode. Wi-Fi power mode to be set.
 * @return ESP_OK. 
 * @return ESP_ERR_INVALID_ARG. 
 *
 */
esp_err_t wifi_set_power_mode(wifi_power_mode_t mode);

#endif /* WIFI_POWER_MANAGER_H*/
