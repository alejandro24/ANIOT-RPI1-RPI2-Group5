#include "nvs.h"
#include "sgp30.h"
#include "nvs_flash.h"

#define storage_get(X)                                            \
    _Generic( (X),                                                \
        sgp30_log_entry_t*  : storage_get_sgp30_log_entry,        \
        thingsboard_url_t*  : storage_get_thingsboard_url         \
    )( (X) )

#define nvs_get(X)                                            \
    _Generic( (X),                                                \
        sgp30_log_entry_t*  : nvs_get_sgp30_log_entry,        \
        thingsboard_url_t*  : nvs_get_thingsboard_url         \
    )( (X) )

typedef void* thingsboard_url_t;

esp_err_t storage_init();
esp_err_t storage_get_sgp30_log_entry(sgp30_log_entry_t *sgp30_log_entry_handle);
esp_err_t storage_get_thingsboard_url(thingsboard_url_t *thingsboard_url);

esp_err_t nvs_init();
esp_err_t nvs_get_sgp30_log_entry(sgp30_log_entry_t *sgp30_log_entry_handle);
esp_err_t nvs_get_thingsboard_url(thingsboard_url_t *thingsboard_url);
