#ifndef SNTP_SYNC_H
#define SNTP_SYNC_H

#include "esp_err.h"
#include <time.h>

void obtain_time(void);

void time_sync_notification_cb(struct timeval *tv);

void init_sntp(void);

#endif