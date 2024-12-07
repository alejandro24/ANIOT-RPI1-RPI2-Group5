#ifndef SNTP_SYNC_H
#define SNTP_SYNC_H

#include <time.h>  // Para manejo de fechas y horas

#ifdef __cplusplus
extern "C" {
#endif

// Función para inicializar y sincronizar el tiempo vía SNTP
void sntp_sync_initialize(const char *server);

// Función para obtener la hora actual como una cadena formateada
void sntp_sync_get_time(char *buffer, size_t buffer_size);

// Callback para la notificación de sincronización de tiempo
void sntp_sync_time_notification(struct timeval *tv);

#ifdef __cplusplus
}
#endif

#endif // SNTP_SYNC_H
