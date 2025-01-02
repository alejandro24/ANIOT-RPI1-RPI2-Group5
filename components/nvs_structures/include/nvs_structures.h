#include "sgp30.h"
#include "nvs_flash.h"

esp_err_t nvs_get_baseline(nvs_handle_t handle, const char *key, sgp30_log_entry_t *out_baseline);
esp_err_t nvs_set_baseline(nvs_handle_t handle, const char *key, sgp30_log_entry_t *baseline);
