#include "nvs_structures.h"
#include "mqtt_controller.h"
#include "nvs.h"
#include "sgp30.h"
#include "esp_err.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "softAP_provision.h"

#define SGP30_STORAGE_NAMESPACE "sgp30"
#define SGP30_NVS_BASELINE_KEY "baseline"
#define SOFTAP_NVS_WIFI_CREDENTIALS_KEY "wifi_credentials"

#define TAG "NVS"
static nvs_handle_t storage_handle;

esp_err_t storage_init() {
    return nvs_open("nvs", NVS_READWRITE, &storage_handle);
}

esp_err_t storage_get_sgp30_timed_measurement(
    sgp30_timed_measurement_t *sgp30_log_entry_handle
) {
    return nvs_get_sgp30_timed_measurement(sgp30_log_entry_handle);
}
esp_err_t storage_get_wifi_credentials(
    wifi_credentials_t *sgp30_log_entry_handle
) {
    return nvs_get_wifi_credentials(sgp30_log_entry_handle);
}

esp_err_t storage_get_thingsboard_url(
    thingsboard_url_t thingsboard_url_handle
) {
    return nvs_get_thingsboard_url(thingsboard_url_handle);
}
esp_err_t nvs_get_wifi_credentials(
    wifi_credentials_t *wifi_credentials
) {
    size_t wifi_credentials_len;
    char* key = SOFTAP_NVS_WIFI_CREDENTIALS_KEY;
    ESP_RETURN_ON_ERROR(
        nvs_get_blob(
            storage_handle,
            key,
            NULL,
            &wifi_credentials_len
        ),
        TAG, "Could not get baseline len from NVS"
    );
    ESP_RETURN_ON_ERROR(
        nvs_get_blob(
            storage_handle,
            key,
            wifi_credentials,
            &wifi_credentials_len
        ),
        TAG, "Could not get baseline from NVS"
    );
    return ESP_OK;
}

esp_err_t nvs_get_sgp30_timed_measurement(
    sgp30_timed_measurement_t *sgp30_log_entry_handle
) {
    size_t baseline_len;
    char* key = SGP30_NVS_BASELINE_KEY;
    ESP_RETURN_ON_ERROR(
        nvs_get_blob(
            storage_handle,
            key,
            NULL,
            &baseline_len
        ),
        TAG, "Could not get baseline len from NVS"
    );
    ESP_RETURN_ON_ERROR(
        nvs_get_blob(
            storage_handle,
            key,
            sgp30_log_entry_handle,
            &baseline_len
        ),
        TAG, "Could not get baseline from NVS"
    );
    return ESP_OK;
}

esp_err_t nvs_get_thingsboard_url(
    thingsboard_url_t thingsboard_url_handle
) {
    size_t thingsboard_url_len;
    char* key = "thingsboard_url";
    ESP_RETURN_ON_ERROR(
        nvs_get_blob(
            storage_handle,
            key,
            NULL,
            &thingsboard_url_len
        ),
        TAG, "Could not get baseline len from NVS"
    );
    ESP_RETURN_ON_ERROR(
        nvs_get_blob(
            storage_handle,
            key,
            thingsboard_url_handle,
            &thingsboard_url_len
        ),
        TAG, "Could not get baseline from NVS"
    );
    return ESP_OK;
}
