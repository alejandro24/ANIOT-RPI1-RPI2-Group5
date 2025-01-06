#include "nvs.h"
#include "sgp30.h"
#include "esp_err.h"
#include "esp_check.h"
#include "nvs_flash.h"


#define TAG "NVS"


esp_err_t nvs_get_sgp30_measurement_queue(
    sgp30_log_t *q,
    nvs_handle_t h)
{
    size_t measurement_size = 0;
    ESP_RETURN_ON_ERROR(
        nvs_get_blob(
            h,
            "measurements",
            NULL,
            &measurement_size
        ),
        TAG, "Could not retrieve measurement size from memory"
    );

    ESP_RETURN_ON_ERROR(
        nvs_get_blob(h, "measurements", q, &measurement_size),
        TAG,
        "Could not retrieve measurements"
    );
    return ESP_OK;
}

esp_err_t nvs_set_sgp30_measurement_queue(
    sgp30_log_t *q,
    nvs_handle_t h)
{
    size_t measurement_size = sizeof(sgp30_log_t);
    ESP_RETURN_ON_ERROR(
        nvs_set_blob(h, "measurements", q, measurement_size),
        TAG,
        "Could not retrieve measurements"
    );
    return ESP_OK;
}

esp_err_t nvs_set_baseline(
    nvs_handle_t handle,
    const char *key,
    const sgp30_log_entry_t *baseline
) {
    ESP_RETURN_ON_ERROR(
        nvs_set_blob(
            handle,
            key,
            baseline,
            sizeof(sgp30_log_entry_t)
        ),
        TAG, "Could not set baseline to NVS"
    );
    return ESP_OK;
}

esp_err_t nvs_get_baseline(
    nvs_handle_t handle,
    const char *key,
    sgp30_log_entry_t *baseline
) {
    size_t baseline_len;
    baseline = NULL;
    ESP_RETURN_ON_ERROR(
        nvs_get_blob(
            handle,
            key,
            NULL,
            &baseline_len
        ),
        TAG, "Could not get baseline len from NVS"
    );
    ESP_RETURN_ON_ERROR(
        nvs_get_blob(
            handle,
            key,
            baseline,
            &baseline_len
        ),
        TAG, "Could not get baseline from NVS"
    );
    return ESP_OK;
}








