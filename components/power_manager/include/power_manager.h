#ifndef POWER_MANAGER_H_
#define POWER_MANAGER_H_

#include "esp_event.h"
#include <time.h>

ESP_EVENT_DECLARE_BASE(POWER_MANAGER_EVENT);

#define POWER_MANAGER_DEEP_SLEEP_EVENT 0

/* Configuración del rango horario activo por defecto*/
#define DEFAULT_START_HOUR 8
#define DEFAULT_END_HOUR 22

/* Duración por defecto si no hay RTC (14 horas activo, 10 en Deep Sleep)*/
#define DEFAULT_ACTIVE_HOURS 14
#define DEFAULT_SLEEP_HOURS 10

/* 1 hora = 36 * 100.000.000 microsegundos*/
#define CONVERSION_HOURS_TO_MICROSECONDS (36ULL * 100 * 1000 * 1000)
/* 1 minuto = 60 * 1.000.000 microsegundos*/
#define CONVERSION_MINUTES_TO_MICROSECONDS (60L * 1000 * 1000)

/**
 * @brief Initial configuration of power manager. It is used if SNTP time is not got correctly.
 * @param 
 * @return
 * 
 */
void power_manager_init();

/**
 * @brief Power manager configuration if SNTP time has been got successfuly.
 * @param Time got from SNTP.
 * @return
 * 
 */
esp_err_t power_manager_set_sntp_time(struct tm *timeinfo);

/**
 * @brief Procedure to switch power option to deep sleep mode..
 * @param 
 * @return
 * 
 */
void power_manager_enter_deep_sleep();

/**
 * @brief Get the wifi credentials from the storage.
 * @param wifi_credentials Pointer to the wifi credentials.
 * @return
 * - ESP_OK: Success
 * - some other error code: Failure
 */
void power_manager_deinit();

#endif /* POWER_MANAGER_H_ */
