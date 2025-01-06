#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"
#include "esp_system.h"
#include <esp_event.h>
#include "cJSON.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt.h"

ESP_EVENT_DEFINE_BASE(MQTT_THINGSBOARD_EVENTS);

static const char *TAG = "mqtt_thingsboard";
esp_mqtt_client_handle_t client;
static QueueHandle_t mqtt_event_queue;  // Cola para manejar eventos
//[NVS]
static char access_token[40];

void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

void received_data(cJSON *root, char* topic){
    cJSON *item; 
    int send_time;
    if(strcmp(topic, "v1/devices/me/attributes") == 0){
        if(cJSON_HasObjectItem(root, "send_time")){
            item = cJSON_GetObjectItem(root, "send_time");
            if(cJSON_IsNumber(item)){
                send_time = item->valueint;
                ESP_ERROR_CHECK(
                    esp_event_post(MQTT_THINGSBOARD_EVENTS, MQTT_NEW_SEND_TIME, &send_time, sizeof(send_time), portMAX_DELAY));
            }
        }
    }
}

bool isProvision(cJSON *root, char* topic){
    cJSON *receive; 
    if(strcmp(topic, "/provision/response") == 0){
        receive = cJSON_GetObjectItem(root, "status"); 
        if(strcmp(receive->valuestring, "SUCCESS") == 0){
            receive = cJSON_GetObjectItem(root, "credentialsValue");
            strcpy(access_token, receive->valuestring);
            return true;
        }
    }
    return false;
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    cJSON *jsonData = NULL;
    char* provision_data;
    char* topic;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        if(strlen(access_token) == 0){
            jsonData = cJSON_CreateObject();
            cJSON_AddItemToObjectCS(jsonData, "provisionDeviceKey", cJSON_CreateStringReference(CONFIG_PROVISION_KEY));
            cJSON_AddItemToObjectCS(jsonData, "provisionDeviceSecret", cJSON_CreateStringReference(CONFIG_SECRET_KEY));

            msg_id = esp_mqtt_client_subscribe(client, "/provision/response", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            provision_data = cJSON_PrintUnformatted(jsonData);
            msg_id = esp_mqtt_client_publish(client, "/provision/request", provision_data, 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            free(provision_data);
            cJSON_Delete(jsonData);
        }
        else{
            ESP_LOGI(TAG, "Conectado con token de acceso: %s", access_token);
            if(mqtt_event_queue != NULL){
                vQueueDelete(mqtt_event_queue);
                mqtt_event_queue = NULL;
            }
            esp_mqtt_client_subscribe(client, "v1/devices/me/attributes", 0);
        }
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
        topic = malloc(sizeof(char)*event->topic_len);
        strncpy(topic, event->topic, event->topic_len);
        topic[event->topic_len] = '\0';
        if(isProvision(jsonData, topic)){
            // Enviar señal a la tarea de MQTT para reconectarse
            mqtt_event_t reconnect_event = MQTT_RECONNECT_WITH_TOKEN;
            xQueueSend(mqtt_event_queue, &reconnect_event, portMAX_DELAY);
        }
        else{
            received_data(jsonData, topic);
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


void mqtt_provision_task(void *pvParameters) {
    mqtt_event_t event;
    char* thingsboard_url = (char*)pvParameters;
    while (1) {
        if (xQueueReceive(mqtt_event_queue, &event, portMAX_DELAY)) {
            if (event == MQTT_RECONNECT_WITH_TOKEN) {
                // Desconectar para volver a conectarse con el token
                esp_mqtt_client_stop(client);
                esp_mqtt_client_destroy(client);
                esp_mqtt_client_config_t mqtt_cfg = {
                    .broker.address.uri = thingsboard_url,
                    .credentials.username = access_token,
                    .broker.address.port = 1883,
                };
                client = esp_mqtt_client_init(&mqtt_cfg);
                /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
                esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
                esp_mqtt_client_start(client);
                ESP_LOGI(TAG, "Reconectado con token de acceso");
                vTaskDelete(NULL);   
            }
        }
    }
}

void mqtt_init(char* thingsboard_url, char* main_access_token)
{
    mqtt_event_queue = xQueueCreate(10, sizeof(mqtt_event_t));
    if(main_access_token != NULL){
        strcpy(access_token, main_access_token);
    }
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = thingsboard_url,
        .broker.address.port = 1883,
        .credentials.username = main_access_token == NULL ? "provision" : access_token,
    };

    xTaskCreate(mqtt_provision_task, "mqtt_task", 4096, thingsboard_url, 5, NULL);
    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}