/**
 * @file sgp30_types.h
 * @brief SGP30 types.
 */
#ifndef SGP30_TYPES_H
#define SGP30_TYPES_H
#include <stdint.h>
#include <time.h>

/**
 * @brief Maximum queue size for sgp30_measurement_log_t.
 */
#define MAX_QUEUE_SIZE 12

/**
 * @brief SGP30 measurement.
 */
typedef struct {
    uint16_t eCO2; /**< Equivalent CO2 */
    uint16_t TVOC; /**< Total Volatile Organic Compounds */
} sgp30_measurement_t;

/**
 * @brief SGP30 measurement log.
 */
typedef struct {
    size_t oldest_index; /**< Index of oldest measure*/
    size_t size; /**< Number of measurements */
    sgp30_measurement_t measurements[MAX_QUEUE_SIZE]; /**< Measurement array */
} sgp30_measurement_log_t;

/**
 * @brief SGP30 timed measurement.
 */
typedef struct {
    sgp30_measurement_t measurement; /**< Measurement */
    time_t time; /**< Time */
} sgp30_timed_measurement_t;
#endif // !SGP30_TYPES_H
