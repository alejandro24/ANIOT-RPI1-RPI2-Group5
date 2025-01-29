#ifndef SNTP_SYNC_H
#define SNTP_SYNC_H

#include "esp_err.h"
#include <time.h>

ESP_EVENT_DECLARE_BASE(SNTP_SYNC_EVENT);

typedef enum {
    SNTP_SUCCESSFULL_SYNC,
}sntp_sync_event_t;

/**
 * @brief Procedure to obtain the current time.
 *
 * @param 
 * @return
 *   
 */
void obtain_time(void);

/**
 * @brief Procedure to notify the success time syncronisation.
 *
 * @param 
 * @return
 *   
 */
void time_sync_notification_cb(struct timeval *tv);

/**
 * @brief Procedure to initialise SNTP to get current time.
 *
 * @param 
 * @return
 *   
 */
void init_sntp(esp_event_loop_handle_t loop);

#endif
