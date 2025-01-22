#ifndef SGP30_H
#define SGP30_H

#include "driver/i2c_types.h"
#include "esp_err.h"
#include "freertos/idf_additions.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "driver/i2c_master.h"
#include <stdint.h>
#include <time.h>
#include "sgp30_types.h"

#define SGP30_I2C_ADDR ((uint8_t) 0x58) /* I2C address of SGP30 sensor */
#define SGP30_CRC_8_POLY ((uint8_t) 0x31) /* CRC-8 generator polynomial */
#define SGP30_CRC_8_INIT ((uint8_t) 0xFF) /* CRC-8 generator polynomial */

#define ZERO_OUT_QUEUE_ON_DEQUEUE

// States for the SGP30 FSM
// - Once the device is returned from the create function it is uninitialized.
typedef enum {
    SGP30_STATE_UNINITIAZED,
    SGP30_STATE_INITIALIZING,
    SGP30_STATE_BASELINE_ACQUISITION,
    SGP30_STATE_FUNCTIONING,
} sgp30_state_t;

typedef struct {
    sgp30_state_t state;
    esp_err_t (*operation) (void);
} sgp30_state_operation_t;

typedef enum {
    SGP30_EVENT_NEW_MEASUREMENT,
    SGP30_EVENT_IAQ_INITIALIZING,
    SGP30_EVENT_IAQ_INITIALIZED,
    SGP30_EVENT_NEW_BASELINE,
    SGP30_EVENT_BASELINE_SET,
    SGP30_EVENT_NEW_INTERVAL,
} sgp30_event_id_t ;

typedef struct {
    sgp30_event_id_t event_id;
    esp_event_handler_t event_handler;
} sgp30_event_handler_register_t;

// SGP30 register write only addresses
// SGP30 register read and write addresses
typedef enum {
    SGP30_REG_INIT_AIR_QUALITY = 0x2003, /* */
    SGP30_REG_MEASURE_AIR_QUALITY = 0x2008, /* */
    SGP30_REG_GET_BASELINE = 0x2015, /* */
    SGP30_REG_SET_BASELINE = 0x201e, /* */
    SGP30_REG_MEASURE_TEST = 0x2032, /* */
    SGP30_REG_GET_FEATURE_SET_VERSION = 0x202f, /* */
    SGP30_REG_MEASURE_RAW_SIGNALS = 0x2050, /* */
    SGP30_REG_SET_HUMIDITY = 0x2061, /* */
    SGP30_REG_GET_SERIAL_ID = 0x3682,
} sgp30_register_rw_t;


ESP_EVENT_DECLARE_BASE(SGP30_EVENT);

bool sgp30_is_baseline_expired(time_t stored, time_t current);

esp_err_t sgp30_measurement_log_get_mean(
    sgp30_measurement_t *m,
    const sgp30_measurement_log_t *q);

esp_err_t sgp30_measurement_log_enqueue(
    const sgp30_measurement_t *m,
    sgp30_measurement_log_t *q);

esp_err_t sgp30_measurement_log_dequeue(
    sgp30_measurement_t *m,
    sgp30_measurement_log_t *q);

/**
 * @brief Creates a handle for the SGP30 device on the specified I2C bus.
 *
 * This function initializes and returns a handle for the SGP30 sensor device
 * connected to the given I2C bus. The device address and communication speed
 * must be specified.
 *
 * @param bus_handle Handle to the I2C bus where the SGP30 device is connected.
 * @param dev_addr I2C address of the SGP30 device.
 * @param dev_speed Communication speed for the I2C device.
 * @return Handle to the SGP30 device, or NULL if creation failed.
 */
esp_err_t sgp30_device_create(
    i2c_master_bus_handle_t bus_handle,
    const uint16_t dev_addr,
    const uint32_t dev_speed);

esp_err_t sgp30_restart_measuring(uint64_t new_measurement_interval_us);
/**
 * @brief Deletes the SGP30 device instance.
 *
 * This function releases any resources associated with the SGP30 device
 * instance identified by the provided I2C master device handle.
 *
 * @param dev_handle The handle to the I2C master device associated with the
 * SGP30 device.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Failed to delete the device
 */
esp_err_t sgp30_device_delete(i2c_master_dev_handle_t dev_handle);
/**
 * UNDOCUMENTED
 */
esp_err_t sgp30_init(
    esp_event_loop_handle_t loop,
    sgp30_measurement_t *baseline
);
/**
 * UNDOCUMENTED
 */
esp_err_t sgp30_start_measuring(uint64_t us);
/**
 * @brief Initiates the measurement capabilities of the SGP30 device.
 *
 * This function has to be executed once before any measurement can be issued
 *
 * @param bus_handle Handle to the I2C bus where the SGP30 device is connected.
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_FAIL: Communication with the sensor failed
 *     - ESP_ERR_INVALID_CRC: Received wrong chechsum
 */
esp_err_t sgp30_init_air_quality(i2c_master_dev_handle_t dev_handle);

/**
 * @brief Retrieve eCO2 and TVOC readings from the SGP30 sensor.
 *
 * This function communicates with the SGP30 sensor over I2C to obtain the current
 * eCO2 and TVOC measurements.
 * Then posts a SENSOR_EVENT_NEW_MEASUREMENT to the sgp30_event_loop
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_FAIL: Communication with the sensor failed
 *     - ESP_ERR_INVALID_CRC: Received wrong chechsum
 */
esp_err_t sgp30_measure_air_quality(i2c_master_dev_handle_t dev_handle, sgp30_measurement_t *new_measurement);
esp_err_t sgp30_get_baseline(i2c_master_dev_handle_t dev_handle, sgp30_measurement_t *baseline);
esp_err_t sgp30_set_baseline(i2c_master_dev_handle_t dev_hanlde, const sgp30_measurement_t *baseline);

esp_err_t sgp30_measure_air_quality_and_post_esp_event();
esp_err_t sgp30_get_baseline_and_post_esp_event();
esp_err_t sgp30_set_baseline_and_post_esp_event();
/**
 * @brief Retrieve the ID from the SGP30 sensor.
 *
 * This function communicates with the SGP30 sensor over I2C to obtain its unique identifier.
 *
 * @param dev_handle Handle to the I2C master device.
 * @param id Pointer to a buffer where the ID will be stored.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Communication with the sensor failed
 *     - ESP_ERR_INVALID_CRC: Received wrong chechsum
 */
esp_err_t sgp30_get_id();
/**
 * TODO DOCUMENTATION
    */

#endif // SGP30_H
