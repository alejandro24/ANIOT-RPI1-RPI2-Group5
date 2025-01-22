#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "cJSON.h"
#include "esp_err.h"
#include "esp_event_base.h"
#include "esp_system.h"
#include "esp_partition.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_log.h"
#include "esp_check.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include "portmacro.h"
#include <sys/param.h>
#include "mqtt_controller.h"
#include "thingsboard_types.h"

#define MAX_ACCESS_TOKEN_LEN 40
#define MAX_PROVISIONING_WAIT portMAX_DELAY
#define THINGSBOARD_PROVISION_USERNAME "provision"
#define DEVICE_ATTRIBUTES_TOPIC "v1/devices/me/attributes"
#define DEVICE_TELEMETRY_TOPIC "v1/devices/me/telemetry"
#define PROVISION_REQUEST_TOPIC "/provision/request"
#define PROVISION_RESPONSE_TOPIC "/provision/response"

static const char *TAG = "mqtt_thingsboard";
esp_event_loop_handle_t event_loop;
esp_mqtt_client_handle_t client;
static SemaphoreHandle_t is_provisioned;  // Cola para manejar eventos
//[NVS]
static char access_token[MAX_ACCESS_TOKEN_LEN];
static size_t access_token_len;

ESP_EVENT_DEFINE_BASE(MQTT_THINGSBOARD_EVENT);

static void mqtt_connected_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    ESP_LOGI(TAG, "Conectado con token de acceso: %s", access_token);
    esp_mqtt_client_subscribe(client, DEVICE_ATTRIBUTES_TOPIC, 0);
}

static void mqtt_disconnected_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
}

static void mqtt_subscribed_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    esp_mqtt_event_handle_t event = event_data;
    ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
}

static void mqtt_unsubscribed_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    esp_mqtt_event_handle_t event = event_data;
    ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
}

static void mqtt_published_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    esp_mqtt_event_handle_t event = event_data;
    ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
}

static void mqtt_data_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    esp_mqtt_event_handle_t event = event_data;
    ESP_LOGI(TAG, "MQTT_EVENT_DATA");
    printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
    printf("DATA=%.*s\r\n", event->data_len, event->data);
    cJSON *jsonData = cJSON_Parse(event->data);
    received_data(jsonData, event->topic, event->topic_len);
    cJSON_Delete(jsonData);
}

static void mqtt_error_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    esp_mqtt_event_handle_t event = event_data;
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
        log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
        log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
        ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
    }
}

static const mqtt_event_handle_register_t mqtt_registered_events[] =
{
    { MQTT_EVENT_CONNECTED, mqtt_connected_event_handler},
    { MQTT_EVENT_DISCONNECTED, mqtt_disconnected_event_handler},
    { MQTT_EVENT_SUBSCRIBED, mqtt_subscribed_event_handler},
    { MQTT_EVENT_UNSUBSCRIBED, mqtt_unsubscribed_event_handler},
    { MQTT_EVENT_PUBLISHED, mqtt_published_event_handler},
    { MQTT_EVENT_DATA, mqtt_data_event_handler},
    { MQTT_EVENT_ERROR, mqtt_error_event_handler}
};

static const size_t mqtt_registered_events_len = sizeof(mqtt_registered_events) / sizeof(mqtt_registered_events[0]);

void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

void received_data(cJSON *root, char* topic, size_t topic_len){
    cJSON *item;
    int send_time;
    if(strncmp(topic, DEVICE_ATTRIBUTES_TOPIC, topic_len) == 0){
        if(cJSON_HasObjectItem(root, "send_time")){
            item = cJSON_GetObjectItem(root, "send_time");
            if(cJSON_IsNumber(item)){
                send_time = item->valueint;
                ESP_ERROR_CHECK(
                    esp_event_post_to(event_loop, MQTT_THINGSBOARD_EVENT, MQTT_NEW_SEND_TIME, &send_time, sizeof(send_time), portMAX_DELAY));
            }
        }
    }
}

bool is_provision(cJSON *root, char* topic, size_t topic_len){
    cJSON *receive;

    if(strncmp(topic, PROVISION_RESPONSE_TOPIC, topic_len) == 0){
        receive = cJSON_GetObjectItem(root, "status");
        if(strcmp(receive->valuestring, "SUCCESS") == 0){
            receive = cJSON_GetObjectItem(root, "credentialsValue");
            mqtt_set_access_token(receive->valuestring, strlen(receive->valuestring));
            return true;
        }
    }
    return false;
}

esp_err_t send_messure(char * data_to_send){
    ESP_ERROR_CHECK(esp_mqtt_client_publish(client, "v1/devices/me/telemetry", data_to_send, 0, 1, 0));

    cJSON_free(data_to_send);

    return ESP_OK;
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    cJSON *jsonData = NULL;
    char* provision_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        jsonData = cJSON_CreateObject();
        cJSON_AddItemToObjectCS(jsonData, "provisionDeviceKey", cJSON_CreateStringReference(CONFIG_PROVISION_KEY));
        cJSON_AddItemToObjectCS(jsonData, "provisionDeviceSecret", cJSON_CreateStringReference(CONFIG_SECRET_KEY));
        msg_id = esp_mqtt_client_subscribe(client, PROVISION_RESPONSE_TOPIC, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        provision_data = cJSON_PrintUnformatted(jsonData);
        msg_id = esp_mqtt_client_publish(client, PROVISION_REQUEST_TOPIC, provision_data, 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        cJSON_free(provision_data);
        cJSON_Delete(jsonData);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        cJSON *jsonData = cJSON_Parse(event->data);
        if(is_provision(jsonData, event->topic, event->topic_len)){
            xSemaphoreGive(is_provisioned);
        }
        else{
            received_data(jsonData, event->topic, event->topic_len);
        }
        cJSON_Delete(jsonData);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}


esp_err_t mqtt_set_access_token(char* token, size_t token_len) {
    if (token_len > MAX_ACCESS_TOKEN_LEN) {
        return ESP_ERR_INVALID_SIZE;
    } else {
        access_token_len = token_len;
        strncpy(access_token, token, token_len);
        return ESP_OK;
    }
}

esp_err_t mqtt_provision(thingsboard_url_t thingsboard_url)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = thingsboard_url,
        .broker.address.port = 8883,
        //.broker.verification.certificate = (const char *)server_pem_start,
        //.credentials.authentication.certificate = (const char *) deviceKey_pem_start,
        //.credentials.authentication.key = (const char *) chain_pem_start,
    };

    is_provisioned = xSemaphoreCreateBinary();

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(
        esp_mqtt_client_register_event(
            client,
            ESP_EVENT_ANY_ID,
            mqtt_event_handler,
            NULL
        ),
        TAG,
        "could not register handler"
    );

    ESP_RETURN_ON_ERROR(
        esp_mqtt_client_start(client),
        TAG,
        "could not start provision client"
    );

    if(xSemaphoreTake(is_provisioned, MAX_PROVISIONING_WAIT) != pdTRUE) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        return ESP_FAIL;
    }

    esp_mqtt_client_stop(client);
    esp_mqtt_client_destroy(client);
    return ESP_OK;
}

esp_err_t mqtt_init(
    esp_event_loop_handle_t loop,
    thingsboard_url_t thingsboard_url,
    char* server_pem,
    char* devicekey_pem,
    char* chain_pem
) {
    event_loop = loop;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = thingsboard_url,
        .broker.address.port = 8883,
        .broker.verification.certificate = server_pem,
        .credentials.authentication.certificate = devicekey_pem,
        .credentials.authentication.key = chain_pem,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        return ESP_FAIL;
    }

    for (int i = 0; i < mqtt_registered_events_len; i++)
    {
        ESP_RETURN_ON_ERROR(
            esp_mqtt_client_register_event(
                client,
                mqtt_registered_events[i].esp_mqtt_event_id,
                mqtt_registered_events[i].event_handler,
                NULL
            ),
            TAG,
            "could not register handler for %d",
            mqtt_registered_events[i].esp_mqtt_event_id
        );
    }

    ESP_RETURN_ON_ERROR(
        esp_mqtt_client_start(client),
        TAG,
        "could not start mqtt client"
    );

    return ESP_OK;
}

esp_err_t mqtt_publish(
    char* data,
    size_t data_len
) {
    int msg_id = esp_mqtt_client_publish(
        client,
        DEVICE_TELEMETRY_TOPIC,
        data,
        data_len,
        0,
        0
    );
    if (msg_id < 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
