#ifndef SNTP_SYNC_H
#define SNTP_SYNC_H

#include "esp_err.h"
#include <time.h>

ESP_EVENT_DECLARE_BASE(SNTP_SYNC_EVENT);

typedef enum {
    SNTP_SUCCESSFULL_SYNC,
}sntp_sync_event_t;

void obtain_time(void);

void time_sync_notification_cb(struct timeval *tv);

void init_sntp(esp_event_loop_handle_t loop);

#endif