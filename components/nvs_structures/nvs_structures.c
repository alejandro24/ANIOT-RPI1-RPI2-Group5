#include "nvs_flash.h"
#include "nvs_structures.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "sgp30_types.h"
#include "softap_provision_types.h"
#include "thingsboard_types.h"

#define NVS_SGP30_STORAGE_NAMESPACE    "sgp30"
#define NVS_SGP30_BASELINE_KEY         "baseline"
#define NVS_WIFI_CREDENTIALS_NAMESPACE "wifi_cred"
#define NVS_WIFI_CREDENTIALS_SSID_KEY  "ssid"
#define NVS_WIFI_CREDENTIALS_PASS_KEY  "pass"
#define NVS_THINGSBOARD_NAMESPACE      "thingsboard"
#define NVS_THINGSBOARD_URI_KEY        "uri"
#define NVS_THINGSBOARD_PORT_KEY       "port"
#define NVS_THINGSBOARD_CACERT_KEY     "ca_cert"
#define NVS_THINGSBOARD_DEVCERT_KEY    "dev_key"
#define NVS_THINGSBOARD_CHAINCERT_KEY  "chain_cert"

#define TAG "NVS"

static esp_err_t nvs_get_thingsboard_cfg(thingsboard_cfg_t *cfg)
{
    nvs_handle_t storage_handle;
    if (nvs_open(NVS_THINGSBOARD_NAMESPACE, NVS_READONLY, &storage_handle)
        != ESP_OK)
    {
        return ESP_FAIL;
    }
    // Get address
    size_t uri_len;
    if (nvs_get_str(storage_handle, NVS_THINGSBOARD_URI_KEY, NULL, &uri_len)
        != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not get thingsboard uri length");
        return ESP_FAIL;
    }
    char *uri = malloc(uri_len);
    if (nvs_get_str(storage_handle, NVS_THINGSBOARD_URI_KEY, uri, &uri_len)
        != ESP_OK)
    {
        nvs_close(storage_handle);
        free(uri);
        ESP_LOGE(TAG, "Could not get thingsboard uri");
        return ESP_FAIL;
    }
    uint16_t port;
    if (nvs_get_u16(storage_handle, NVS_THINGSBOARD_PORT_KEY, &port) != ESP_OK)
    {
        nvs_close(storage_handle);
        free(uri);
        ESP_LOGE(TAG, "Could not get thingsboard port");
        return ESP_FAIL;
    }

    // Get verification
    size_t ca_cert_len;
    if (nvs_get_str(
            storage_handle,
            NVS_THINGSBOARD_CACERT_KEY,
            NULL,
            &ca_cert_len
        )
        != ESP_OK)
    {
        nvs_close(storage_handle);
        free(uri);
        ESP_LOGE(TAG, "Could not get thingsboard ca certificate length");
        return ESP_FAIL;
    }
    char *ca_cert = calloc(1, ca_cert_len + 1);
    if (nvs_get_str(
            storage_handle,
            NVS_THINGSBOARD_CACERT_KEY,
            ca_cert,
            &ca_cert_len
        )
        != ESP_OK)
    {
        nvs_close(storage_handle);
        free(uri);
        free(ca_cert);
        ESP_LOGE(TAG, "Could not get thingsboard ca certificate");
        return ESP_FAIL;
    }
    // Get credentials
    size_t dev_cert_len;
    if (nvs_get_str(
            storage_handle,
            NVS_THINGSBOARD_DEVCERT_KEY,
            NULL,
            &dev_cert_len
        )
        != ESP_OK)
    {
        nvs_close(storage_handle);
        free(uri);
        free(ca_cert);
        ESP_LOGE(TAG, "Could not get thingsboard device certificate length");
        return ESP_FAIL;
    }
    char *dev_cert = calloc(1, dev_cert_len + 1);
    if (nvs_get_str(
            storage_handle,
            NVS_THINGSBOARD_DEVCERT_KEY,
            dev_cert,
            &dev_cert_len
        )
        != ESP_OK)
    {
        nvs_close(storage_handle);
        free(uri);
        free(ca_cert);
        free(dev_cert);
        ESP_LOGE(TAG, "Could not get thingsboard device certificate");
        return ESP_FAIL;
    }
    size_t chain_cert_len;
    if (nvs_get_str(
            storage_handle,
            NVS_THINGSBOARD_CHAINCERT_KEY,
            NULL,
            &chain_cert_len
        )
        != ESP_OK)
    {
        nvs_close(storage_handle);
        free(uri);
        free(ca_cert);
        free(dev_cert);
        ESP_LOGE(TAG, "Could not get thingsboard chain certificate length");
        return ESP_FAIL;
    }
    char *chain_cert = calloc(1, chain_cert_len + 1);
    if (nvs_get_str(
            storage_handle,
            NVS_THINGSBOARD_CHAINCERT_KEY,
            chain_cert,
            &chain_cert_len
        )
        != ESP_OK)
    {
        nvs_close(storage_handle);
        free(uri);
        free(ca_cert);
        free(dev_cert);
        free(chain_cert);
        ESP_LOGE(TAG, "Could not get thingsboard chain certificate");
        return ESP_FAIL;
    }

    nvs_close(storage_handle);

    cfg->address.uri = uri;
    cfg->address.port = port;
    cfg->verification.certificate = ca_cert;
    cfg->verification.certificate_len = ca_cert_len + 1;
    cfg->credentials.authentication.certificate = chain_cert;
    cfg->credentials.authentication.certificate_len = chain_cert_len + 1;
    cfg->credentials.authentication.key = dev_cert;
    cfg->credentials.authentication.key_len = dev_cert_len + 1;

    return ESP_OK;
}

static esp_err_t nvs_set_thingsboard_cfg(const thingsboard_cfg_t *thingsboard_cfg)
{
    nvs_handle_t storage_handle;
    esp_err_t err;

    // Open namespace
    err = nvs_open(NVS_THINGSBOARD_NAMESPACE, NVS_READWRITE, &storage_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not open thingsboard namespace to write");
        return err;
    }

    // Set address
    err = nvs_set_str(
        storage_handle,
        NVS_THINGSBOARD_URI_KEY,
        thingsboard_cfg->address.uri
    );
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not store thingsboard uri");
        return err;
    }

    err = nvs_set_u16(
        storage_handle,
        NVS_THINGSBOARD_PORT_KEY,
        thingsboard_cfg->address.port
    );
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not store thingsboard port");
        return err;
    }

    // Set verification
    err = nvs_set_str(
        storage_handle,
        NVS_THINGSBOARD_CACERT_KEY,
        thingsboard_cfg->verification.certificate
    );
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not store thingsboard verification certificate");
        return err;
    }

    // Set credentials
    err = nvs_set_str(
        storage_handle,
        NVS_THINGSBOARD_DEVCERT_KEY,
        thingsboard_cfg->credentials.authentication.certificate
    );
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);

        ESP_LOGE(
            TAG,
            "Could not store thingsboard credentials authentication "
            "certificate"
        );
        return err;
    }

    err = nvs_set_str(
        storage_handle,
        NVS_THINGSBOARD_CHAINCERT_KEY,
        thingsboard_cfg->credentials.authentication.key
    );
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(
            TAG,
            "Could not store thingsboard credentials authentication key"
        );
        return err;
    }

    // Commit and close
    err = nvs_commit(storage_handle);
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not commit changes in nvs");
        return err;
    }
    nvs_close(storage_handle);
    return ESP_OK;
}
static esp_err_t nvs_set_sgp30_baseline(
    const sgp30_timed_measurement_t *timed_measurement
)
{
    nvs_handle_t storage_handle;
    esp_err_t err;

    // open namespace
    err =
        nvs_open(NVS_SGP30_STORAGE_NAMESPACE, NVS_READWRITE, &storage_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not open sgp30 namespace to write");
        return err;
    }
    err = nvs_set_blob(
        storage_handle,
        NVS_SGP30_BASELINE_KEY,
        (void *)timed_measurement,
        sizeof(sgp30_timed_measurement_t)
    );
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not store thingsboard port");
        return err;
    }

    // Commit and close
    err = nvs_commit(storage_handle);
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not commit changes in nvs");
        return err;
    }
    nvs_close(storage_handle);
    return ESP_OK;
}

static esp_err_t nvs_set_wifi_credentials(const wifi_credentials_t *wifi_credentials)
{
    nvs_handle_t storage_handle;
    esp_err_t err;

    // open namespace
    err = nvs_open(
        NVS_WIFI_CREDENTIALS_NAMESPACE,
        NVS_READWRITE,
        &storage_handle
    );
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not open wifi credentials namespace to write");
        return err;
    }

    // Set SSID
    err = nvs_set_str(
        storage_handle,
        NVS_WIFI_CREDENTIALS_SSID_KEY,
        wifi_credentials->ssid
    );
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not store wifi credentials ssid");
        return err;
    }

    // Set password
    err = nvs_set_str(
        storage_handle,
        NVS_WIFI_CREDENTIALS_PASS_KEY,
        wifi_credentials->password
    );
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not store wifi credentials password");
        return err;
    }

    err = nvs_commit(storage_handle);
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not commit changes in nvs");
        return err;
    }
    nvs_close(storage_handle);
    return ESP_OK;
}

static esp_err_t nvs_get_wifi_credentials(wifi_credentials_t *wifi_credentials)
{
    nvs_handle_t storage_handle;
    esp_err_t err;

    // Open handle
    if (nvs_open(NVS_WIFI_CREDENTIALS_NAMESPACE, NVS_READONLY, &storage_handle)
        != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not open wifi credentials namespace");
        return ESP_FAIL;
    }

    // Get SSID
    size_t wifi_credentials_ssid_len;
    err = nvs_get_str(
        storage_handle,
        NVS_WIFI_CREDENTIALS_SSID_KEY,
        NULL,
        &wifi_credentials_ssid_len
    );
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not get wifi credentials ssid len from NVS");
        return err;
    }
    err = nvs_get_str(
        storage_handle,
        NVS_WIFI_CREDENTIALS_SSID_KEY,
        wifi_credentials->ssid,
        &wifi_credentials_ssid_len
    );
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not get wifi credentials ssid from NVS");
        return err;
    }

    // Get pass
    size_t wifi_credentials_pass_len;
    err = nvs_get_str(
        storage_handle,
        NVS_WIFI_CREDENTIALS_PASS_KEY,
        NULL,
        &wifi_credentials_pass_len
    );
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not get wifi credentials pass len from NVS");
        return err;
    }
    err = nvs_get_str(
        storage_handle,
        NVS_WIFI_CREDENTIALS_PASS_KEY,
        wifi_credentials->password,
        &wifi_credentials_pass_len
    );
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not get wifi credentials pass from NVS");
        return err;
    }
    // Close handle
    nvs_close(storage_handle);

    return ESP_OK;
}
static esp_err_t nvs_get_sgp30_baseline(
    sgp30_timed_measurement_t *sgp30_timed_measurement_handle
)
{
    nvs_handle_t storage_handle;
    esp_err_t err;
    err = nvs_open(NVS_SGP30_STORAGE_NAMESPACE, NVS_READONLY, &storage_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not open sgp30 namespace in NVS");
        return err;
    }
    // Get
    size_t timed_measurement_len;
    err = nvs_get_blob(
        storage_handle,
        NVS_SGP30_BASELINE_KEY,
        NULL,
        &timed_measurement_len
    );
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not get baseline len from NVS");
        return err;
    }

    err = nvs_get_blob(
        storage_handle,
        NVS_SGP30_BASELINE_KEY,
        sgp30_timed_measurement_handle,
        &timed_measurement_len
    );
    if (err != ESP_OK)
    {
        nvs_close(storage_handle);
        ESP_LOGE(TAG, "Could not get baseline from NVS");
        return err;
    }

    nvs_close(storage_handle);
    return ESP_OK;
}

esp_err_t storage_init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not initialize NVS");
        return err;
    }
    return ESP_OK;
}

esp_err_t storage_erase()
{
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not erase NVS");
        return err;
    }
    return ESP_OK;
}

esp_err_t storage_get_sgp30_baseline(
    sgp30_timed_measurement_t *sgp30_log_entry_handle
)
{
    return nvs_get_sgp30_baseline(sgp30_log_entry_handle);
}

esp_err_t storage_set_sgp30_baseline(
    const sgp30_timed_measurement_t *timed_measurement
)
{
    return nvs_set_sgp30_baseline(timed_measurement);
}

esp_err_t storage_get_wifi_credentials(wifi_credentials_t *sgp30_log_entry_handle)
{
    return nvs_get_wifi_credentials(sgp30_log_entry_handle);
}

esp_err_t storage_set_wifi_credentials(const wifi_credentials_t *wifi_credentials)
{
    return nvs_set_wifi_credentials(wifi_credentials);
}

esp_err_t storage_get_thingsboard_cfg(thingsboard_cfg_t *thingsboard_cfg)
{
    return nvs_get_thingsboard_cfg(thingsboard_cfg);
}

esp_err_t storage_set_thingsboard_cfg(const thingsboard_cfg_t *thingsboard_cfg)
{
    return nvs_set_thingsboard_cfg(thingsboard_cfg);
}
