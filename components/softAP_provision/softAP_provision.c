#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_err.h"
#include "esp_check.h"
#include "cJSON.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>

#include <wifi_provisioning/manager.h>

#include <wifi_provisioning/scheme_softap.h>
#include "lwip/err.h"
#include "lwip/sys.h"
#include "thingsboard_types.h"
#include "softAP_provision.h"

static const char *TAG = "softAP_provisioning";
wifi_credentials_t provision_wifi_credentials;
thingsboard_cfg_t provision_thingsboard_cfg;
static SemaphoreHandle_t is_provisioned;
int data_to_receive = 0;

#if CONFIG_EXAMPLE_PROV_SECURITY_VERSION_2
esp_err_t example_get_sec2_salt(const char **salt, uint16_t *salt_len) {
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
    ESP_LOGI(TAG, "Development mode: using hard coded salt");
    *salt = sec2_salt;
    *salt_len = sizeof(sec2_salt);
    return ESP_OK;
#elif CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
    ESP_LOGE(TAG, "Not implemented!");
    return ESP_FAIL;
#endif
}

esp_err_t example_get_sec2_verifier(const char **verifier, uint16_t *verifier_len) {
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
    ESP_LOGI(TAG, "Development mode: using hard coded verifier");
    *verifier = sec2_verifier;
    *verifier_len = sizeof(sec2_verifier);
    return ESP_OK;
#elif CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
    /* This code needs to be updated with appropriate implementation to provide verifier */
    ESP_LOGE(TAG, "Not implemented!");
    return ESP_FAIL;
#endif
}
#endif

void provision_event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
#ifdef CONFIG_EXAMPLE_RESET_PROV_MGR_ON_FAILURE
    static int retries;
#endif
    switch (event_id) {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            break;
        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
            ESP_LOGI(TAG, "Received Wi-Fi credentials"
                        "\n\tSSID     : %s\n\tPassword : %s",
                        (const char *) wifi_sta_cfg->ssid,
                        (const char *) wifi_sta_cfg->password);
            strncpy(provision_wifi_credentials.ssid, (const char *) wifi_sta_cfg->ssid, sizeof(provision_wifi_credentials.ssid) - 1);
            provision_wifi_credentials.ssid[sizeof(provision_wifi_credentials.ssid) - 1] = '\0'; // Asegurar terminación en null
            strncpy(provision_wifi_credentials.password, (const char *) wifi_sta_cfg->password, sizeof(provision_wifi_credentials.password) - 1);
            provision_wifi_credentials.password[sizeof(provision_wifi_credentials.password) - 1] = '\0'; // Asegurar terminación en null
            break;
        }
        case WIFI_PROV_CRED_FAIL: {
            wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
            ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
                        "\n\tPlease reset to factory and retry provisioning",
                        (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                        "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
#ifdef CONFIG_EXAMPLE_RESET_PROV_MGR_ON_FAILURE
            retries++;
            if (retries >= CONFIG_EXAMPLE_PROV_MGR_MAX_RETRY_CNT) {
                ESP_LOGI(TAG, "Failed to connect with provisioned AP, resetting provisioned credentials");
                wifi_prov_mgr_reset_sm_state_on_failure();
                retries = 0;
            }
#endif
            break;
        }
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            xSemaphoreGive(is_provisioned);
#ifdef CONFIG_EXAMPLE_RESET_PROV_MGR_ON_FAILURE
            retries = 0;
#endif
            break;
        case WIFI_PROV_END:
            /* De-initialize manager once provisioning is finished */
            wifi_prov_mgr_deinit();
            break;
        default:
            break;
    }
}

void wifi_init_sta(void)
{
    /* Start Wi-Fi in station mode */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

esp_err_t data_to_receive_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                          uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    if (inbuf)
        data_to_receive = atoi((char*) inbuf);
    else
        return ESP_FAIL;

    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    if (*outbuf == NULL) {
        ESP_LOGE(TAG, "System out of memory");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1; /* +1 for NULL terminating byte */

    return ESP_OK;
}

esp_err_t thingsboard_cnf_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                          uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    if (inbuf) {
        ESP_LOGI(TAG, "Received data: %.*s", inlen, (char *)inbuf);
        if(data_to_receive == 0){
            provision_thingsboard_cfg.address.uri = malloc(sizeof(char) * inlen);
            strncpy(provision_thingsboard_cfg.address.uri, (char*) inbuf, inlen);
        }
        else if(data_to_receive == 1){
            provision_thingsboard_cfg.address.port = atoi((char*) inbuf);
        }
        else if(data_to_receive == 2){
            provision_thingsboard_cfg.verification.certificate_len = inlen;
            provision_thingsboard_cfg.verification.certificate = malloc(sizeof(char) * inlen);
            strncpy(provision_thingsboard_cfg.verification.certificate, (char*) inbuf, inlen);
        }
        else if(data_to_receive == 3){
            provision_thingsboard_cfg.credentials.authentication.certificate_len = inlen;
            provision_thingsboard_cfg.credentials.authentication.certificate = malloc(sizeof(char) * inlen);
            strncpy(provision_thingsboard_cfg.credentials.authentication.certificate, (char*) inbuf, inlen);
        }
        else if(data_to_receive == 4){
            provision_thingsboard_cfg.credentials.authentication.key_len = inlen;
            provision_thingsboard_cfg.credentials.authentication.key = malloc(sizeof(char) * inlen);
            strncpy(provision_thingsboard_cfg.credentials.authentication.key, (char*) inbuf, inlen);
        }
        else {
            return ESP_FAIL;
        }
    }
    else{
        return ESP_FAIL;
    }

    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    if (*outbuf == NULL) {
        ESP_LOGE(TAG, "System out of memory");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1; /* +1 for NULL terminating byte */

    return ESP_OK;
}

thingsboard_cfg_t get_thingsboard_cfg(){
    return provision_thingsboard_cfg;
}

wifi_credentials_t get_wifi_credentials(){
    return provision_wifi_credentials;
}

esp_err_t softAP_provision_init(thingsboard_cfg_t *thingsboard_cfg, wifi_credentials_t *wifi_credentials ){
    
    is_provisioned = xSemaphoreCreateBinary();
    
    /* Initialize TCP/IP */
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Error iniciando TCP/IP");

    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &provision_event_handler, NULL), TAG, "Fallo en creacion del handler");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &provision_event_handler, NULL), TAG, "Fallo en creacion del handler");

    /* Initialize Wi-Fi including netif with default config */
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "Error iniciando la configuracion Wifi");

    if(thingsboard_cfg == NULL || wifi_credentials == NULL){ 
        /* Configuration for the provisioning manager */
        wifi_prov_mgr_config_t config = {
            .scheme = wifi_prov_scheme_softap,
            /*This can be set to
            * WIFI_PROV_EVENT_HANDLER_NONE when using wifi_prov_scheme_softap*/
            .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
        };
        /* Initialize provisioning manager with the
        * configuration parameters set above */
        ESP_RETURN_ON_ERROR(wifi_prov_mgr_init(config),TAG, "Fallo iniciando el provisionamiento");
        
        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));   

    #ifdef CONFIG_EXAMPLE_PROV_SECURITY_VERSION_1
            /* What is the security level that we want (1, 2):
            *      - WIFI_PROV_SECURITY_0 is simply plain text communication.
            *      - WIFI_PROV_SECURITY_1 is secure communication which consists of secure handshake
            *          using X25519 key exchange and proof of possession (pop) and AES-CTR
            *          for encryption/decryption of messages.
            *      - WIFI_PROV_SECURITY_2 SRP6a based authentication and key exchange
            *        + AES-GCM encryption/decryption of messages
            */
            wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

            /* Do we want a proof-of-possession (ignored if Security 0 is selected):
            *      - this should be a string with length > 0
            *      - NULL if not used
            */
            const char *pop = "abcd1234";

            /* This is the structure for passing security parameters
            * for the protocomm security 1.
            */
            wifi_prov_security1_params_t *sec_params = pop;

            const char *username  = NULL;

    #elif CONFIG_EXAMPLE_PROV_SECURITY_VERSION_2
            wifi_prov_security_t security = WIFI_PROV_SECURITY_2;
            /* The username must be the same one, which has been used in the generation of salt and verifier */
            /* This is the structure for passing security parameters
            * for the protocomm security 2.
            * If dynamically allocated, sec2_params pointer and its content
            * must be valid till WIFI_PROV_END event is triggered.
            */
            wifi_prov_security2_params_t sec2_params = {};

            ESP_RETURN_ON_ERROR(example_get_sec2_salt(&sec2_params.salt, &sec2_params.salt_len),TAG, "Fallo al obtener el salt");
            ESP_RETURN_ON_ERROR(example_get_sec2_verifier(&sec2_params.verifier, &sec2_params.verifier_len),TAG, "Fallo al obtener verificacion");

            wifi_prov_security2_params_t *sec_params = &sec2_params;
    #endif

        /* What is the service key (could be NULL)
        * This translates to :
        *     - Wi-Fi password when scheme is wifi_prov_scheme_softap
        *          (Minimum expected length: 8, maximum 64 for WPA2-PSK)
        *     - simply ignored when scheme is wifi_prov_scheme_ble
        */
        const char *service_key = NULL;

        /* An endpoint that applications create to get the
        *thingsboard url.
        * This call must be made before starting the provisioning.
        */
        ESP_ERROR_CHECK(wifi_prov_mgr_endpoint_create("data-to-receive"));
        ESP_ERROR_CHECK(wifi_prov_mgr_endpoint_create("thingsboard-cnf"));

        /* Start provisioning service */
        ESP_RETURN_ON_ERROR(wifi_prov_mgr_start_provisioning(security, (const void *) sec_params, service_name, service_key), TAG, "Fallo al empezar el provisionamiento");

        /* The handler for the optional endpoint created above.
        * This call must be made after starting the provisioning, and only if the endpoint
        * has already been created above.
        */
        ESP_ERROR_CHECK(wifi_prov_mgr_endpoint_register("data-to-receive", data_to_receive_prov_data_handler, NULL));
        ESP_ERROR_CHECK(wifi_prov_mgr_endpoint_register("thingsboard-cnf", thingsboard_cnf_prov_data_handler, NULL));

        if(xSemaphoreTake(is_provisioned, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "Provisioning failed");
        }
    }
    else{
        wifi_config_t wifi_config = {
            .sta = {
                    .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                },
        };
        memcpy(wifi_config.sta.ssid, wifi_credentials->ssid, strlen(wifi_credentials->ssid));
        memcpy(wifi_config.sta.password, wifi_credentials->password, strlen(wifi_credentials->password));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_LOGI(TAG, "wifi_init_sta finished.");
    }

    return ESP_OK;
}
