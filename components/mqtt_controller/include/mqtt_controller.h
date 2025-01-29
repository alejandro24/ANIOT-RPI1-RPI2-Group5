#ifndef MQTT_CONTROLLER_H
#define MQTT_CONTROLLER_H
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "esp_event_base.h"
#include "mqtt_client.h"
#include "thingsboard_types.h"

ESP_EVENT_DECLARE_BASE(MQTT_THINGSBOARD_EVENT);

typedef enum {
    MQTT_NEW_SEND_TIME,
} mqtt_thingsboard_event_t;

typedef struct {
    esp_mqtt_event_id_t esp_mqtt_event_id;
    esp_event_handler_t event_handler;
} mqtt_event_handle_register_t;

void log_error_if_nonzero(const char *message, int error_code);

/*Function to work with the data received from the subscribed topics*/
void received_data(cJSON *root, char* topic, size_t len);

/*Function to check if the device is being provision with his access token in this case the access token is store*/
bool is_provision(cJSON *root, char* topic, size_t len);

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

/*Function that waits to be provision with access token and create the new mqtt client conection*/
void mqtt_provision_task(void *pvParameters);

/*esp_err_t mqtt_provision(thingsboard_url_t thingsboard_url);*/
esp_err_t mqtt_init(esp_event_loop_handle_t loop, thingsboard_cfg_t *cfg);
esp_err_t mqtt_publish(char* data, size_t data_len);
#endif /*MQTT_CONTROLLER_H*/
