#ifndef NVS_STRUCTURES_H
#define NVS_STRUCTURES_H
#include "softap_provision_types.h"
#include "sgp30_types.h"
#include "thingsboard_types.h"
#include "nvs_flash.h"

#define storage_get(X)                                            \
    _Generic( (X),                                                \
        sgp30_timed_measurement_t*  : storage_get_sgp30_timed_measurement,        \
        thingsboard_cfg_t* : storage_get_thingboard_cfg, \
        wifi_credentials_t* : storage_get_wifi_credentials   \
    )( (X) )

#define nvs_get(X)                                            \
    _Generic( (X),                                                \
        sgp30_timed_measurement_t*  : nvs_get_sgp30_timed_measurement,        \
        thingsboard_cfg_t* : nvs_get_thingboard_cfg, \
        wifi_credentials_t* : nvs_get_wifi_credentials \
    )( (X) )

#define nvs_set(X)                                            \
    _Generic( (X),                                                \
        sgp30_timed_measurement_t*  : nvs_set_sgp30_timed_measurement,        \
        thingsboard_cgf_t* : nvs_set_thingboard_cfg, \
        wifi_credentials_t* : nvs_set_wifi_credentials \
    )( (X) )

esp_err_t storage_init();
esp_err_t storage_get_sgp30_timed_measurement(sgp30_timed_measurement_t *sgp30_timed_measurement_handle);
esp_err_t storage_get_thingboard_cfg(thingsboard_cfg_t *thingsboard_cfg);
esp_err_t storage_get_wifi_credentials(wifi_credentials_t *wifi_credentiasl);

esp_err_t nvs_init();
esp_err_t nvs_get_sgp30_timed_measurement(sgp30_timed_measurement_t *sgp30_timed_measurement_handle);
esp_err_t nvs_get_thingboard_cfg(thingsboard_cfg_t *thingsboard_cfg);
esp_err_t nvs_get_wifi_credentials(wifi_credentials_t *wifi_credentiasl);

esp_err_t nvs_init();
esp_err_t nvs_set_sgp30_timed_measurement(const sgp30_timed_measurement_t *sgp30_timed_measurement_handle);
esp_err_t nvs_set_thingboard_cfg(const thingsboard_cfg_t *thingsboard_address);
esp_err_t nvs_set_wifi_credentials(const wifi_credentials_t *wifi_credentiasl);
#endif // !NVS_STRUCTURES_H
