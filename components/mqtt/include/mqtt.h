#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "cJSON.h"

ESP_EVENT_DEFINE_BASE(MQTT_THINGSBOARD_EVENTS);

typedef enum {
    MQTT_RECONNECT_WITH_TOKEN,
    MQTT_NEW_SEND_TIME,
} mqtt_thingsboard_event_t;

void log_error_if_nonzero(const char *message, int error_code);

//Function to work with the data received from the subscribed topics
void received_data(cJSON *root, char* topic);

//Function to check if the device is being provision with his access token in this case the access token is store
bool isProvision(cJSON *root, char* topic);

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

//Function that waits to be provision with access token and create the new mqtt client conection
void mqtt_provision_task(void *pvParameters);

void mqtt_init(char* thingsboard_url, char* main_access_token);