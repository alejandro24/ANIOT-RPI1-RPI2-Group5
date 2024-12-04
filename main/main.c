#include "wifi_manager.h"

void app_main(void) {
    // Inicializa la gestión Wi-Fi
    wifi_manager_init();

    // Conectar al Wi-Fi con los parámetros deseados
    wifi_manager_connect("MIOT", "MIOT_WIFI_2024!");
}