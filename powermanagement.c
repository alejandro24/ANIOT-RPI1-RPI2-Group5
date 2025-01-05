#include <time.h>
#include <sys/time.h>
#include "esp_pm.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"

#define START_HOUR 8
#define END_HOUR 22
#define WORK_DURATION_HOURS 14
#define SLEEP_DURATION_HOURS 10

// Callback para configurar PM
void configure_pm() {
    esp_pm_config_esp32_t pm_config = {
        .max_freq_mhz = 80,
        .min_freq_mhz = 10,
        .light_sleep_enable = true
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
}

// Función para obtener la hora actual
struct tm get_current_time() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo;
}

// Función principal
void app_main() {
    configure_pm();

    struct tm timeinfo = get_current_time();
    int current_hour = timeinfo.tm_hour;
    bool entering_deep_sleep = false;
    
    // Verificar si hay RTC
    if (current_hour >= 0 && current_hour <= 23) {
        // Comparar con el rango horario especificado
        if (current_hour >= START_HOUR && current_hour < END_HOUR) {
            printf("Funcionando normalmente...\n");
        } else {
            entering_deep_sleep = true;
        }
    } else {
        // Si no hay RTC, usar la lógica del tiempo desde el arranque
        int uptime_hours = esp_timer_get_time() / (1000000 * 3600);
        if (uptime_hours < WORK_DURATION_HOURS) {
            printf("Funcionando normalmente...\n");
        } else {
            entering_deep_sleep = true;
        }
    }

    // Entrar en Deep Sleep si es necesario
    if (entering_deep_sleep) {
        printf("Entrando en Deep Sleep...\n");
        // Configurar duración de Deep Sleep
        esp_sleep_enable_timer_wakeup(SLEEP_DURATION_HOURS * 3600 * 1000000ULL);
        esp_deep_sleep_start();
    }
}
