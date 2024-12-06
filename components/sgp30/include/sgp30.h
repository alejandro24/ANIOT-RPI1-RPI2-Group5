#ifndef SGP30_H
#define SGP30_H

#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "driver/i2c_master.h"
#include <stdint.h>

#define SGP30_I2C_ADDR ((uint8_t) 0x58) /* I2C address of SGP30 sensor */
#define SGP30_CRC_8_POLY ((uint8_t) 0x31) /* CRC-8 generator polynomial */

typedef enum {
    SENSOR_EVENT_NEW_MEASUREMENT,
    SENSOR_IAQ_INITIALIZING,
    SENSOR_IAQ_INITIALIZED,
    SENSOR_GOT_BASELINE,
} mox_event_id_t ;
// SGP30 register write only addresses
typedef enum {
    SGP30_REG_GET_SERIAL_ID = 0x3682,
} sgp30_register_w_t ;

// SGP30 register read and write addresses
typedef enum {
    SGP30_REG_INIT_AIR_QUALITY = 0x2003, /* */
    SGP30_REG_MEASURE_AIR_QUALITY = 0x2008, /* */
    SGP30_REG_GET_BASELINE = 0x2015, /* */
    SGP30_REG_SET_BASELINE = 0x201e, /* */
    SGP30_REG_SET_HUMIDITY = 0x2061, /* */
    SGP30_REG_MEASURE_TEST = 0x2032, /* */
    SGP30_REG_GET_FEATURE_SET_VERSION = 0x202f, /* */
    SGP30_REG_MEASURE_RAW_SIGNALS = 0x2050, /* */
} sgp30_register_rw_t;

typedef struct {
    uint16_t eCO2;
    uint16_t TVOC;
} sgp30_measurement_t;

typedef struct {
    sgp30_measurement_t baseline;
    int64_t timestamp;
} sgp30_baseline_t;

ESP_EVENT_DECLARE_BASE(SENSOR_EVENTS);

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
esp_err_t sgp30_init_air_quality();

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
esp_err_t sgp30_measure_air_quality();

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
esp_err_t sgp30_get_id(i2c_master_dev_handle_t dev_handle, uint8_t *id);
/**
 * TODO DOCUMENTATION
    */
esp_err_t sgp30_get_baseline();

#endif
