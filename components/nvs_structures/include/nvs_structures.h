#include "nvs.h"
#include "sgp30.h"
#include "nvs_flash.h"

#define storage_get(X)                                            \
    _Generic( (X),                                                \
        sgp30_timed_measurement_t*  : storage_get_sgp30_timed_measurement,        \
        thingsboard_url_t*  : storage_get_thingsboard_url,         \
        wifi_credentials_t* : storage_get_wifi_credentials
    )( (X) )

#define nvs_get(X)                                            \
    _Generic( (X),                                                \
        sgp30_timed_measurement_t*  : nvs_get_sgp30_timed_measurement,        \
        thingsboard_url_t*  : nvs_get_thingsboard_url,         \
        wifi_credentials_t* : nvs_get_wifi_credentials
    )( (X) )

typedef void* thingsboard_url_t;

esp_err_t storage_init();
esp_err_t storage_get_sgp30_timed_measurement(sgp30_timed_measurement_t *sgp30_timed_measurement_handle);
esp_err_t storage_get_thingsboard_url(thingsboard_url_t *thingsboard_url);
esp_err_t storage_get_wifi_credentials(wifi_credentials_t *wifi_credentiasl);

esp_err_t nvs_init();
esp_err_t nvs_get_sgp30_timed_measurement(sgp30_timed_measurement_t *sgp30_timed_measurement_handle);
esp_err_t nvs_get_thingsboard_url(thingsboard_url_t *thingsboard_url);
esp_err_t nvs_get_wifi_credentials(wifi_credentials_t *wifi_credentiasl);
