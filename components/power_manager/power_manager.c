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
    // Obtener el timestamp final cuando el temporizador se dispara
    int64_t end_time = esp_timer_get_time();

    // Calcular el tiempo transcurrido (en microsegundos)
    int64_t elapsed_time = end_time - start_time;

    // Mostrar el tiempo transcurrido en segundos
    ESP_LOGI(TAG, "Tiempo de ejecución del temporizador: %lld microsegundos", elapsed_time);

    esp_event_post(POWER_MANAGER_EVENT, POWER_MANAGER_DEEP_SLEEP_EVENT, NULL, 0, portMAX_DELAY);
}

void power_manager_enter_deep_sleep()
{
    ESP_LOGI(TAG, "Entrando en deep_sleep.");
    esp_deep_sleep_start();
}

void power_manager_init()
{
// Configurar gestor energia para entrar automaticamente en light_sleep
#if CONFIG_PM_ENABLE
    // Configure dynamic frequency scaling:
    // maximum and minimum frequencies are set in sdkconfig,
    // automatic light sleep is enabled if tickless idle support is enabled.
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 160, // ESP32c3: 160 MHz, ESP32-devkit-c: 240 MHz
        .min_freq_mhz = 80,  // 80 MHz
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        .light_sleep_enable = true
#endif
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
    ESP_LOGI(TAG, "Configurado gestor de energia automatico");
#endif // CONFIG_PM_ENABLE

    uint64_t sleep_hours = DEFAULT_SLEEP_HOURS * CONVERSION_HOURS_TO_MICROSECONDS;
    uint64_t active_hours = DEFAULT_ACTIVE_HOURS * CONVERSION_HOURS_TO_MICROSECONDS;

    /* Configurar timer para despertar de deep_sleep cada 10 horas */
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(sleep_hours));
    ESP_LOGI(TAG, "Configurado wakeup por timer en %d horas", (DEFAULT_SLEEP_HOURS));

    /* Configurar timer para entrar en modo deep_sleep cada 14 horas */
    esp_timer_create_args_t deep_sleep_timer_args = {
        .callback = deep_sleep_timer_callback,
        .name = "deep_sleep_timer"};

    ESP_ERROR_CHECK(esp_timer_create(&deep_sleep_timer_args, &deep_sleep_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(deep_sleep_timer, active_hours));
    ESP_LOGI(TAG, "Configurado deep_sleep en %d horas", (DEFAULT_ACTIVE_HOURS));
}

esp_err_t power_manager_set_sntp_time(struct tm *timeinfo)
{
    esp_err_t errcode = ESP_OK;

    int64_t wakeup_time_in_minutes = 0;     // tiempo durmiendo hasta que salte el wakeup
    int64_t time_till_sleep_in_minutes = 0; // tiempo restante hasta acabar el rango activo

    bool enter_deep_sleep_now = false;

    /* --- Tenemos hora SNTP, deshabilitar anteriores timers y wakeup source ---
     */
    if (esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER) != ESP_OK) // Try to deactivate timer trigger only
        errcode = esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);   // disable all

    ESP_RETURN_ON_ERROR(errcode, TAG, "Error desactivando el timer de wakeup por defecto");

    errcode = esp_timer_stop(deep_sleep_timer);
    ESP_RETURN_ON_ERROR(errcode, TAG, "Error parando el timer de entrar en deep_sleep por defecto");

    /* --- Habilitar con los nuevos valores calculados usando la hora de SNTP ---
     */
    // Obtener las horas de inicio y fin desde las configuraciones de Kconfig
    const char *start_time_str = CONFIG_PM_ACTIVE_START_HOUR;
    const char *end_time_str = CONFIG_PM_ACTIVE_END_HOUR;

    // Convertir las cadenas de tiempo (HH:MM) a horas y minutos
    int start_hour, start_minute;
    int end_hour, end_minute;

    // Parsear la hora de inicio
    sscanf(start_time_str, "%2d:%2d", &start_hour, &start_minute);
    // Parsear la hora de fin
    sscanf(end_time_str, "%2d:%2d", &end_hour, &end_minute);

    // Modulo con 24 para evitar las 24h y quedarnos con 00h
    start_hour = start_hour % 24;
    end_hour = end_hour % 24;

    // Convertir la hora actual a minutos del día para hacer la comparación más fácil
    int64_t current_time_in_minutes = timeinfo->tm_hour * 60 + timeinfo->tm_min;
    int64_t start_time_in_minutes = start_hour * 60 + start_minute;
    int64_t end_time_in_minutes = end_hour * 60 + end_minute;

    // Contador para el tiempo total en rango activo
    int64_t total_active_time_in_minutes = 0;

    ESP_LOGI(TAG, "Configurando power manager con rango activo: %s - %s.",
             start_time_str, end_time_str);

    // Rango activo cruzado, p.ej desde las 22:00 hasta las 08:00
    if (start_time_in_minutes > end_time_in_minutes)
    {
        total_active_time_in_minutes =
            (24 * 60) - start_time_in_minutes + end_time_in_minutes;

        if (current_time_in_minutes >= start_time_in_minutes ||
            current_time_in_minutes < end_time_in_minutes)
        {
            // Estamos en rango activo, configurar timer para que nos avise llegada la
            // hora de dormir

            // Hora actual menor que hora de fin, estamos en el dia
            if (current_time_in_minutes < end_time_in_minutes)
                time_till_sleep_in_minutes = end_time_in_minutes - current_time_in_minutes;
            else // Hora actual mayor que hora de fin, estamos en el dia anterior
                time_till_sleep_in_minutes = (24 * 60) - current_time_in_minutes + end_time_in_minutes;

            // Calculamos el tiempo del timer de wakeup
            // (minutos totales del dia - minutos_rango_activo)
            wakeup_time_in_minutes = (24 * 60) - total_active_time_in_minutes;
        }
        else
        {
            // Estamos fuera del rango activo, entrar forzadamente en deep_sleep
            // Calcular el tiempo hasta el inicio del rango horario siguiente

            // Hora actual menor que hora de fin, estamos en el dia
            if (current_time_in_minutes < start_time_in_minutes)
                wakeup_time_in_minutes = start_time_in_minutes - current_time_in_minutes;
            else // Hora actual mayor que hora de fin, estamos en el dia anterior
                wakeup_time_in_minutes = (24 * 60) - current_time_in_minutes + start_time_in_minutes;

            enter_deep_sleep_now = true;
        }
    }
    else // Rango activo normal, p.ej desde las 08:00 hasta las 22:00
    {
        total_active_time_in_minutes = end_time_in_minutes - start_time_in_minutes;

        if (current_time_in_minutes >= start_time_in_minutes &&
            current_time_in_minutes < end_time_in_minutes)
        {
            // Estamos en rango activo, configurar timer para que nos avise cuando sea
            // la hora de dormir
            time_till_sleep_in_minutes = end_time_in_minutes - current_time_in_minutes;

            // Calculamos el tiempo del timer de wakeup
            //  (minutos totales del dia - minutos_rango_activo)
            wakeup_time_in_minutes = (24 * 60) - total_active_time_in_minutes;
        }
        else
        {
            // Estamos fuera del rango activo, entrar forzadamente en deep_sleep
            // Calcular el tiempo hasta el inicio del rango horario siguiente
            wakeup_time_in_minutes = start_time_in_minutes - current_time_in_minutes;

            if (wakeup_time_in_minutes < 0)
            {
                // Ajustar si es negativo (es decir, si la hora actual es mayor que la hora de inicio)
                wakeup_time_in_minutes += 24 * 60;
            }

            enter_deep_sleep_now = true;
        }
    }

    uint64_t wkup_time_us = (uint64_t)(wakeup_time_in_minutes * CONVERSION_MINUTES_TO_MICROSECONDS);
    uint64_t time_till_sleep_us = (uint64_t)(time_till_sleep_in_minutes * CONVERSION_MINUTES_TO_MICROSECONDS);

    ESP_LOGI(TAG, "Configurado wakeup tras deep_sleep en %" PRId64 " horas y %" PRId64 " minutos (%" PRId64 " minutos, %" PRIu64 " us).",
             wakeup_time_in_minutes / 60, wakeup_time_in_minutes % 60, wakeup_time_in_minutes, wkup_time_us);

    errcode = esp_sleep_enable_timer_wakeup(wkup_time_us); // En microsegundos
    ESP_RETURN_ON_ERROR(errcode, TAG, "Error activando el nuevo timer de wakeup por rango horario");

    start_time = esp_timer_get_time(); // Tiempo antes de iniciar el timer de deep_sleep, algo funciona mal y no se que es, vamos a comprobarlo

    if (enter_deep_sleep_now)
        power_manager_enter_deep_sleep();
    else
    { // Habilitar timer para que nos avise cuando sea hora de entrar en deep_sleep
        errcode = esp_timer_start_once(deep_sleep_timer, time_till_sleep_us);
        ESP_RETURN_ON_ERROR(errcode, TAG, "Error activando el nuevo timer de entrar en deep_sleep por rango horario");

        ESP_LOGI(TAG, "Tiempo hasta entrar en deep_sleep: %" PRId64 " horas y %" PRId64 " minutos (%" PRId64 " minutos, %" PRIu64 " us).",
                 time_till_sleep_in_minutes / 60, time_till_sleep_in_minutes % 60, time_till_sleep_in_minutes, time_till_sleep_us);
    }

    return errcode;
}