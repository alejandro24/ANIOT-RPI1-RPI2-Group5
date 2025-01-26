/**
 * @file nvs_structures.h
 * @brief NVS structures.
 */

#ifndef NVS_STRUCTURES_H
#define NVS_STRUCTURES_H
#include "esp_err.h"
#include "sgp30_types.h"
#include "softap_provision_types.h"
#include "thingsboard_types.h"

#define storage_get(X)                                                        \
    _Generic (                                                                \
        (X),                                                                  \
        sgp30_timed_measurement_t* : storage_get_sgp30_baseline ,             \
        thingsboard_cfg_t* : storage_get_thingsboard_cfg ,                    \
        wifi_credentials_t* : storage_get_wifi_credentials                    \
    ) ( (X) )

#define storage_set(X)                                                        \
    _Generic (                                                                \
        (X),                                                                  \
        const sgp30_timed_measurement_t* : storage_set_sgp30_baseline ,       \
        const thingsboard_cfg_t* : storage_set_thingsboard_cfg ,              \
        const wifi_credentials_t* : storage_set_wifi_credentials              \
    ) ( (X) )
/**
 * @brief Initialize the storage.
 * @return
 * - ESP_OK: Success
 * - some other error code: Failure
 */
esp_err_t storage_init();
/**
 * @brief Get the sgp30 baseline from the storage.
 * @param baseline Pointer to the sgp30 baseline.
 * @return
 * - ESP_OK: Success
 * - some other error code: Failure
 */
esp_err_t storage_get_sgp30_baseline (
    sgp30_timed_measurement_t *baseline
);

/**
 * @brief Set the sgp30 baseline in the storage.
 * @param baseline Pointer to the sgp30 baseline.
 * @return
 * - ESP_OK: Success
 * - some other error code: Failure
 */
esp_err_t storage_set_sgp30_baseline(
    const sgp30_timed_measurement_t *baseline
);
/**
 * @brief Get the thingsboard configuration from the storage.
 * @param thingsboard_cfg Pointer to the thingsboard configuration.
 * @return
 * - ESP_OK: Success
 * - some other error code: Failure
 */
esp_err_t storage_get_thingsboard_cfg (thingsboard_cfg_t *thingsboard_cfg);
/**
* @brief Set the thingsboard configuration in the storage.
* @param thingsboard_cfg Pointer to the thingsboard configuration.
* @return
* - ESP_OK: Success
* - some other error code: Failure
*/
esp_err_t storage_set_thingsboard_cfg (const thingsboard_cfg_t *thingsboard_cfg);
/**
 * @brief Get the wifi credentials from the storage.
 * @param wifi_credentials Pointer to the wifi credentials.
 * @return
 * - ESP_OK: Success
 * - some other error code: Failure
 */
esp_err_t storage_get_wifi_credentials (wifi_credentials_t *wifi_credentiasl);
/**
 * @brief Set the wifi credentials in the storage.
 * @param wifi_credentials Pointer to the wifi credentials.
 * @return
 * - ESP_OK: Success
 * - some other error code: Failure
 */
esp_err_t storage_set_wifi_credentials (const wifi_credentials_t *wifi_credentiasl);
#endif // !NVS_STRUCTURES_H
