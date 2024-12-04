// wifi_manager/include/wifi_manager.h

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

void wifi_manager_init(void);  // Inicializa la conexión Wi-Fi
void wifi_manager_connect(const char *ssid, const char *password);  // Conecta al Wi-Fi con SSID y contraseña
void wifi_manager_reconnect(void);  // Función para reconectar automáticamente si la conexión se pierde

#endif // WIFI_MANAGER_H
