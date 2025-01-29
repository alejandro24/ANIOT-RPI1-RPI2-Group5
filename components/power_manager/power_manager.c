#include <stdio.h>
#include <string.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_event.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <esp_system.h>
#include <esp_err.h>
#include <esp_check.h>

#include "power_manager.h"

static const char *TAG = "POWER_MANAGER";

ESP_EVENT_DEFINE_BASE(POWER_MANAGER_EVENT);

static esp_timer_handle_t deep_sleep_timer;

static int64_t start_time = 0;

static void deep_sleep_timer_callback(void *arg)
{
    /*Get the final timestamp when the timer is triggered*/

    int64_t end_time = esp_timer_get_time();

    /*  Calculate elapsed time (in microseconds)*/
    int64_t elapsed_time = end_time - start_time;

    /* Show elapsed time in seconds*/
    ESP_LOGI(TAG, "Timer execution time: %lld microseconds", elapsed_time);

    esp_event_post(POWER_MANAGER_EVENT, POWER_MANAGER_DEEP_SLEEP_EVENT, NULL, 0, portMAX_DELAY);
}

void power_manager_enter_deep_sleep()
{
    ESP_LOGI(TAG, "Entering deep_sleep.");
    esp_deep_sleep_start();
}

void power_manager_init()
{
/*Configure energy manager to automatically enter light_sleep*/
#if CONFIG_PM_ENABLE
    /* Configure dynamic frequency scaling:
       maximum and minimum frequencies are set in sdkconfig,
       automatic light sleep is enabled if tickless idle support is enabled.
    */
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 160, // ESP32c3: 160 MHz, ESP32-devkit-c: 240 MHz
        .min_freq_mhz = 80,  // 80 MHz
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        .light_sleep_enable = true
#endif
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
    ESP_LOGI(TAG, "Configured automatic power manager");
#endif /* CONFIG_PM_ENABLE*/

    uint64_t sleep_hours = DEFAULT_SLEEP_HOURS * CONVERSION_HOURS_TO_MICROSECONDS;
    uint64_t active_hours = DEFAULT_ACTIVE_HOURS * CONVERSION_HOURS_TO_MICROSECONDS;

    /* Set timer to wake up from deep_sleep every 10 hours */
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(sleep_hours));
    ESP_LOGI(TAG, "Configured wakeup by timer in %d hours", (DEFAULT_SLEEP_HOURS));

    /* Set timer to enter deep_sleep mode every 14 hours */
    esp_timer_create_args_t deep_sleep_timer_args = {
        .callback = deep_sleep_timer_callback,
        .name = "deep_sleep_timer"};

    ESP_ERROR_CHECK(esp_timer_create(&deep_sleep_timer_args, &deep_sleep_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(deep_sleep_timer, active_hours));
    ESP_LOGI(TAG, "Set deep_sleep to %d hours", (DEFAULT_ACTIVE_HOURS));
}

esp_err_t power_manager_set_sntp_time(struct tm *timeinfo)
{
    esp_err_t errcode = ESP_OK;

    int64_t wakeup_time_in_minutes = 0;     /* time sleeping until wakeup occurs*/
    int64_t time_till_sleep_in_minutes = 0; /*remaining time until end of active range*/

    bool enter_deep_sleep_now = false;

    /*  We have SNTP time, disable previous timers and wakeup source */
    if (esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER) != ESP_OK) /* Try to deactivate timer trigger only*/
        errcode = esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);   /* disable all*/

    ESP_RETURN_ON_ERROR(errcode, TAG, "Error disabling the default wakeup timer");

    errcode = esp_timer_stop(deep_sleep_timer);
    ESP_RETURN_ON_ERROR(errcode, TAG, "Error stopping the timer from entering deep_sleep by default");

    /* Enable with new values ​​calculated using SNTP time
       Get start and end times from Kconfig settings*/
    const char *start_time_str = CONFIG_PM_ACTIVE_START_HOUR;
    const char *end_time_str = CONFIG_PM_ACTIVE_END_HOUR;

    /* Convert time strings (HH:MM) to hours and minutes*/
    int start_hour, start_minute;
    int end_hour, end_minute;

    /* Parse start time*/
    sscanf(start_time_str, "%2d:%2d", &start_hour, &start_minute);
    /* Parse the end time*/
    sscanf(end_time_str, "%2d:%2d", &end_hour, &end_minute);

    /* Module with 24 to avoid 24 hours and stay with 00 hours*/
    start_hour = start_hour % 24;
    end_hour = end_hour % 24;

    /* Convert current time to minutes of day to make comparison easier*/
    int64_t current_time_in_minutes = timeinfo->tm_hour * 60 + timeinfo->tm_min;
    int64_t start_time_in_minutes = start_hour * 60 + start_minute;
    int64_t end_time_in_minutes = end_hour * 60 + end_minute;

    /* Counter for total time in active range*/
    int64_t total_active_time_in_minutes = 0;

    ESP_LOGI(TAG, "Configuring power manager with active range: %s - %s.",
             start_time_str, end_time_str);

    /* Cross active range, e.g. from 22:00 to 08:00*/
    if (start_time_in_minutes > end_time_in_minutes)
    {
        total_active_time_in_minutes =
            (24 * 60) - start_time_in_minutes + end_time_in_minutes;

        if (current_time_in_minutes >= start_time_in_minutes ||
            current_time_in_minutes < end_time_in_minutes)
        {
            /* We are in active range, configure timer to notify us when it is time to sleep*/

            /* Current time less than end time, we are on the day*/
            if (current_time_in_minutes < end_time_in_minutes)
                time_till_sleep_in_minutes = end_time_in_minutes - current_time_in_minutes;
            else /* Current time greater than end time, we are on the previous day*/
                time_till_sleep_in_minutes = (24 * 60) - current_time_in_minutes + end_time_in_minutes;

            /* We calculate the time of the wakeup timer
             (total minutes of the day - minutes_active_range)*/
            wakeup_time_in_minutes = (24 * 60) - total_active_time_in_minutes;
        }
        else
        {
            /* We are out of active range, force deep_sleep
               Calculate the time until the start of the next time range*/

            /* Current time less than end time, we are on the day*/
            if (current_time_in_minutes < start_time_in_minutes)
                wakeup_time_in_minutes = start_time_in_minutes - current_time_in_minutes;
            else /* Current time greater than end time, we are on the previous day*/
                wakeup_time_in_minutes = (24 * 60) - current_time_in_minutes + start_time_in_minutes;

            enter_deep_sleep_now = true;
        }
    }
    else /* Normal active range, e.g. from 08:00 to 22:00*/
    {
        total_active_time_in_minutes = end_time_in_minutes - start_time_in_minutes;

        if (current_time_in_minutes >= start_time_in_minutes &&
            current_time_in_minutes < end_time_in_minutes)
        {
            /* We are in active range, set timer to notify us when it is time to sleep*/
            time_till_sleep_in_minutes = end_time_in_minutes - current_time_in_minutes;

            /* We calculate the time of the wakeup timer 
            (total minutes of the day - minutes_active_range)*/
            wakeup_time_in_minutes = (24 * 60) - total_active_time_in_minutes;
        }
        else
        {
            /* We are out of active range, force deep_sleep*/
            /* Calculate the time until the start of the next time range*/
            wakeup_time_in_minutes = start_time_in_minutes - current_time_in_minutes;

            if (wakeup_time_in_minutes < 0)
            {
                /* Set if negative (i.e. if current time is greater than start time)*/
                wakeup_time_in_minutes += 24 * 60;
            }

            enter_deep_sleep_now = true;
        }
    }

    uint64_t wkup_time_us = (uint64_t)(wakeup_time_in_minutes * CONVERSION_MINUTES_TO_MICROSECONDS);
    uint64_t time_till_sleep_us = (uint64_t)(time_till_sleep_in_minutes * CONVERSION_MINUTES_TO_MICROSECONDS);

    ESP_LOGI(TAG, "Configured wakeup after deep_sleep to %" PRId64 " hours and %" PRId64 " minutes (%" PRId64 " minutes, %" PRIu64 " us).",
             wakeup_time_in_minutes / 60, wakeup_time_in_minutes % 60, wakeup_time_in_minutes, wkup_time_us);

    errcode = esp_sleep_enable_timer_wakeup(wkup_time_us); /* In microsecondss*/
    ESP_RETURN_ON_ERROR(errcode, TAG, "Error activating the new wakeup timer by time range");

    start_time = esp_timer_get_time(); /* Time before starting the deep_sleep timer, something is wrong and I don't know what it is, let's check it*/

    if (enter_deep_sleep_now)
        power_manager_enter_deep_sleep();
    else
    { /*Enable timer to notify us when it is time to enter deep_sleep*/ 
        errcode = esp_timer_start_once(deep_sleep_timer, time_till_sleep_us);
        ESP_RETURN_ON_ERROR(errcode, TAG, "Error activating the new timer to enter deep_sleep due to time range");

        ESP_LOGI(TAG, "Time until entering deep_sleep: %" PRId64 " hours and %" PRId64 " minutes (%" PRId64 " minutes, %" PRIu64 " us)",
                 time_till_sleep_in_minutes / 60, time_till_sleep_in_minutes % 60, time_till_sleep_in_minutes, time_till_sleep_us);
    }

    return errcode;
}
