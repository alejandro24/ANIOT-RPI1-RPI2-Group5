// wifi_manager/src/wifi_manager.c

#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_manager";

// Configuración Wi-Fi
static wifi_config_t wifi_config = {
    .sta = {
        .ssid = "your_ssid",   // SSID de la red Wi-Fi
        .password = "your_password", // Contraseña de la red Wi-Fi
    },
};

// Maneja los eventos de conexión/desconexión
static esp_err_t event_handler(void *ctx, system_event_t *event) {
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "Iniciando conexión Wi-Fi...");
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Conectado a %s", wifi_config.sta.ssid);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "Desconectado, intentando reconectar...");
            esp_wifi_connect();
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Inicializa el Wi-Fi
void wifi_manager_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *esp_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));  // Establecer modo estación (STA)
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// Conecta al Wi-Fi con los parámetros especificados
void wifi_manager_connect(const char *ssid, const char *password) {
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    ESP_LOGI(TAG, "Conectando a %s...", ssid);
    esp_wifi_disconnect();
    esp_wifi_connect();
}

// Reconexión automática en caso de desconexión
void wifi_manager_reconnect(void) {
    esp_wifi_connect();
}
