#include "sntp_sync.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include <stdio.h>

static const char *TAG = "SNTP_SYNC";

#define SNTP_MAX_RETRIES 10
#define SNTP_RETRY_DELAY 2000 / portTICK_PERIOD_MS  // Retraso entre intentos (en ms)

// Callback para notificación de sincronización
void sntp_sync_time_notification(struct timeval *tv) {
    ESP_LOGI(TAG, "Time synchronized: %ld seconds since epoch", tv->tv_sec);
}

// Verificar si el dispositivo está conectado a la red WiFi
bool is_wifi_connected() {
    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    return (err == ESP_OK);  // Si hay una conexión activa a un AP, el estado será ESP_OK
}

// Inicializar y sincronizar el reloj a través de SNTP
void sntp_sync_initialize(const char *server) {
    ESP_LOGI(TAG, "Initializing SNTP...");

    // Inicializar NVS (necesario para la configuración de WiFi)
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());

    // Verificar si estamos conectados a WiFi
    if (!is_wifi_connected()) {
        ESP_LOGE(TAG, "No WiFi connection, cannot synchronize time!");
        return;
    }

    // Configurar SNTP
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(server);
    config.sync_cb = sntp_sync_time_notification;  // Callback opcional
    esp_netif_sntp_init(&config);

    ESP_LOGI(TAG, "Starting SNTP...");
    esp_netif_sntp_start();

    // Esperar la sincronización
    time_t now;
    struct tm timeinfo;
    int retry = 0;
    while (esp_netif_sntp_sync_wait(SNTP_RETRY_DELAY) == ESP_ERR_TIMEOUT && retry < SNTP_MAX_RETRIES) {
        ESP_LOGI(TAG, "Waiting for time synchronization... (%d/%d)", retry + 1, SNTP_MAX_RETRIES);
        retry++;
    }

    if (retry == SNTP_MAX_RETRIES) {
        ESP_LOGE(TAG, "Failed to synchronize time!");
        // Establecer una hora por defecto si no se logra sincronizar
        time(&now);  // Hora por defecto: tiempo del sistema
        localtime_r(&now, &timeinfo);
        ESP_LOGI(TAG, "Using default time: %s", asctime(&timeinfo));
    } else {
        time(&now);  // Si la sincronización fue exitosa
        localtime_r(&now, &timeinfo);
        ESP_LOGI(TAG, "Time synchronized: %s", asctime(&timeinfo));
    }

    // Limpieza (opcional)
    esp_netif_sntp_deinit();
}

// Obtener la hora actual como cadena
void sntp_sync_get_time(char *buffer, size_t buffer_size) {
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    // Formatear la hora como cadena
    strftime(buffer, buffer_size, "%c", &timeinfo);
}
