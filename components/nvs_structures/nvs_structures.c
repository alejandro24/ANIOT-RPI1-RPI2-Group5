#include "nvs_structures.h"
#include "mqtt_controller.h"
#include "nvs.h"
#include "sgp30.h"
#include "esp_err.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "softAP_provision.h"
#include "thingsboard_types.h"
#include <stdlib.h>

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

esp_err_t nvs_set_thingboard_cfg(const thingsboard_cfg_t *thingsboard_cfg) {
    // Set address
    ESP_RETURN_ON_ERROR(
        nvs_set_str(
            storage_handle,
            "tb_uri",
            thingsboard_cfg->address.uri
        ),
        TAG,
        "Could not store thingsboard url"
    );
    ESP_RETURN_ON_ERROR(
        nvs_set_u8(
            storage_handle,
            "tb_port",
            thingsboard_cfg->address.port
        ),
        TAG,
        "Could not store thingsboard port"
    );

    // Set verification
    ESP_RETURN_ON_ERROR(
        nvs_set_str(
            storage_handle,
            "tb_veri_cert",
            thingsboard_cfg->verification.certificate
        ),
        TAG,
        "Could not store thingsboard verification certificate"
    );

    // Set credentials
    ESP_RETURN_ON_ERROR(
        nvs_set_str(
            storage_handle,
            "tb_c_a_cert",
            thingsboard_cfg->credentials.authentication.certificate
        ),
        TAG,
        "Could not store thingsboard credentials authentication certificate"
    );
    ESP_RETURN_ON_ERROR(
        nvs_set_str(
            storage_handle,
            "tb_c_a_key",
            thingsboard_cfg->credentials.authentication.key
        ),
        TAG,
        "Could not store thingsboard credentials authentication key"
    );
    return ESP_OK;
}

esp_err_t storage_get_thingboard_cfg(thingsboard_cfg_t *thingsboard_cfg)
{
    return nvs_get_thingsboard_cfg(thingsboard_cfg);
}

esp_err_t nvs_get_thingsboard_cfg(thingsboard_cfg_t *cfg)
{
    //Get address
    size_t uri_len;
    if (nvs_get_str(storage_handle, "tb_uri", NULL, &uri_len) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not get thingsboard uri length");
        return ESP_FAIL;
    }
    char* uri = malloc(uri_len);
    if (nvs_get_str(storage_handle, "tb_uri", uri, &uri_len) != ESP_OK)
    {
        free(uri);
        ESP_LOGE(TAG, "Could not get thingsboard uri");
        return ESP_FAIL;
    }
    uint8_t port;
    if (nvs_get_u8(storage_handle, "tb_port", &port) != ESP_OK)
    {
        free(uri);
        ESP_LOGE(TAG, "Could not get thingsboard port");
        return ESP_FAIL;
    }

    //Get verification
    size_t ca_cert_len;
    if (nvs_get_str(storage_handle, "tb_cacert", NULL, &ca_cert_len) != ESP_OK)
    {
        free(uri);
        ESP_LOGE(TAG, "Could not get thingsboard ca certificate length");
        return ESP_FAIL;
    }
    char* ca_cert = malloc(ca_cert_len);
    if (nvs_get_str(storage_handle, "tb_cacert", ca_cert, &ca_cert_len) != ESP_OK)
    {
        free(uri);
        free(ca_cert);
        ESP_LOGE(TAG, "Could not get thingsboard ca certificate");
        return ESP_FAIL;
    }
    //Get credentials
    size_t dev_cert_len;
    if (nvs_get_str(storage_handle, "tb_dev_cert", NULL, &dev_cert_len) != ESP_OK)
    {
        free(uri);
        free(ca_cert);
        ESP_LOGE(TAG, "Could not get thingsboard device certificate length");
        return ESP_FAIL;
    }
    char* dev_cert = malloc(dev_cert_len);
    if (nvs_get_str(storage_handle, "tb_dev_cert", dev_cert, &dev_cert_len) != ESP_OK)
    {
        free(uri);
        free(ca_cert);
        free(dev_cert);
        ESP_LOGE(TAG, "Could not get thingsboard device certificate");
        return ESP_FAIL;
    }
    size_t chain_cert_len;
    if (nvs_get_str(storage_handle, "tb_chain_cert", NULL, &chain_cert_len) != ESP_OK)
    {
        free(uri);
        free(ca_cert);
        free(dev_cert);
        ESP_LOGE(TAG, "Could not get thingsboard chain certificate length");
        return ESP_FAIL;
    }
    char* chain_cert = malloc(chain_cert_len);
    if (nvs_get_str(storage_handle, "tb_chain_cert", chain_cert, &chain_cert_len) != ESP_OK)
    {
        free(uri);
        free(ca_cert);
        free(dev_cert);
        free(chain_cert);
        ESP_LOGE(TAG, "Could not get thingsboard chain certificate");
        return ESP_FAIL;
    }

    cfg->address.uri = uri;
    cfg->address.port = port;
    cfg->verification.certificate = ca_cert;
    cfg->credentials.authentication.certificate = dev_cert;
    cfg->credentials.authentication.key = chain_cert;
    return ESP_OK;
}

esp_err_t nvs_set_wifi_credentials(const wifi_credentials_t *wifi_credentials_t) {
    ESP_RETURN_ON_ERROR(
        nvs_set_str(
            storage_handle,
            "wf_ssid",
            wifi_credentials_t->ssid
        ),
        TAG,
        "Could not store wifi ssid"
    );
    ESP_RETURN_ON_ERROR(
        nvs_set_str(
            storage_handle,
            "wf_pass",
            wifi_credentials_t->password
        ),
        TAG,
        "Could not store wifi password"
    );
    return ESP_OK;
}
