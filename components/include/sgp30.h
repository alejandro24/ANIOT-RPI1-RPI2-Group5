#ifndef SPG30_H
#define SPG30_H

#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "driver/i2c_master.h"

#define SPG30_I2C_ADDR ((uint8_t)0x58) /* I2C address of SPG30 sensor */

typedef enum {
    SENSOR_EVENT_DISTANCE_NEW,
} mox_event_id_t ;

// SGP30 register addreses
typedef enum {
    SGP30_REG_INIT_AIR_QUALITY = 0x2003, /* */
    SGP30_REG_MEASURE_AIR_QUALITY = 0x2008, /* */
    SGP30_REG_GET_BASELINE = 0x2015, /* */
    SGP30_REG_SET_BASELINE = 0x201e, /* */
    SGP30_REG_SET_HUMIDITY = 0x2061, /* */
    SGP30_REG_MEASURE_TEST = 0x2032, /* */
    SGP30_REG_GET_FEATURE_SET_VERSION = 0x202f, /* */
    SGP30_REG_MEASURE_RAW_SIGNALS = 0x2050, /* */
} sgp30_register_addr_t;

ESP_EVENT_DECLARE_BASE(SENSOR_EVENTS);

/**
 * @brief Creates a handle for the SPG30 device on the specified I2C bus.
 *
 * This function initializes and returns a handle for the SPG30 sensor device
 * connected to the given I2C bus. The device address and communication speed
 * must be specified.
 *
 * @param bus_handle Handle to the I2C bus where the SPG30 device is connected.
 * @param dev_addr I2C address of the SPG30 device.
 * @param dev_speed Communication speed for the I2C device.
 * @return Handle to the SPG30 device, or NULL if creation failed.
 */
i2c_master_dev_handle_t sgp30_device_create(
    i2m_master_bus_handle_t bus_handle,
    const uint16_t dev_addr,
    const uint32_t dev_speed);

/**
 * @brief Deletes the SPG30 device instance.
 *
 * This function releases any resources associated with the SPG30 device
 * instance identified by the provided I2C master device handle.
 *
 * @param dev_handle The handle to the I2C master device associated with the 
 * SPG30 device.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Failed to delete the device
 */
esp_err_t sgp30_device_delete(i2c_master_dev_handle_t dev_handle);

/**
 * @brief Retrieve temperature and humidity readings from the SPG30 sensor.
 *
 * This function communicates with the SPG30 sensor over I2C to obtain the current
 * temperature and humidity measurements.
 *
 * @param[in] dev_handle Handle to the I2C master device.
 * @param[in] reg Register to read from.
 * @param[out] data1 Pointer to a float where the temperature reading will be stored.
 * @param[out] data2 Pointer to a float where the humidity reading will be stored.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_FAIL: Communication with the sensor failed
 */
esp_err_t sgp30_get_th(
    i2c_master_dev_handle_t dev_handle,
    shtc3_register_rw_t reg,
    float *data1,
    float *data2);

/**
 * @brief Retrieve the ID from the SPG30 sensor.
 *
 * This function communicates with the SPG30 sensor over I2C to obtain its unique identifier.
 *
 * @param dev_handle Handle to the I2C master device.
 * @param id Pointer to a buffer where the ID will be stored.
 * 
 * @return 
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Communication with the sensor failed
 */
esp_err_t sgp30_get_id(i2c_master_dev_handle_t dev_handle, uint8_t *id);

#endif
