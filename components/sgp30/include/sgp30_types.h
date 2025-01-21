#ifndef SGP30_TYPES_H
#define SGP30_TYPES_H
#include <stdint.h>
#include <time.h>
#define MAX_QUEUE_SIZE 12

typedef struct {
    uint16_t eCO2;
    uint16_t TVOC;
} sgp30_measurement_t;

typedef struct {
    size_t oldest_index;
    size_t size;
    sgp30_measurement_t measurements[MAX_QUEUE_SIZE];
} sgp30_measurement_log_t;

typedef struct {
    sgp30_measurement_t measurement;
    time_t time;
} sgp30_timed_measurement_t;
#endif // !SGP30_TYPES_H
