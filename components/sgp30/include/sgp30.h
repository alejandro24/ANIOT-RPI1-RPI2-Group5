#ifndef SGP30_H
#define SGP30_H

#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "freertos/idf_additions.h"
#include "nvs_flash.h"
#include "sgp30_types.h"
#include <stdint.h>
#include <time.h>

#define SGP30_I2C_ADDR   ((uint8_t)0x58) /* I2C address of SGP30 sensor */
#define SGP30_CRC_8_POLY ((uint8_t)0x31) /* CRC-8 generator polynomial */
#define SGP30_CRC_8_INIT ((uint8_t)0xFF) /* CRC-8 generator polynomial */

#define ZERO_OUT_QUEUE_ON_DEQUEUE

/**
 * @brief SGP30 state machine states.
 */
typedef enum
{
    SGP30_STATE_UNINITIAZED,          /*!< Device is uninitialized */
    SGP30_STATE_INITIALIZING,         /*!< Device is initializing */
    SGP30_STATE_BASELINE_ACQUISITION, /*!< Device is acquiring baseline */
    SGP30_STATE_FUNCTIONING,          /*!< Device is functioning */
} sgp30_state_t;

/**
 * @brief State machine operations.
 */
typedef struct
{
    sgp30_state_t state;          /*!< State to which the operation applies */
    esp_err_t (*operation)(void); /*!< Operation to be executed */
} sgp30_state_operation_t;

/**
 * @brief SGP30 event IDs.
 * This events are to used to notify the application of the ocurrence of
 * different events in the SGP30 module.
 */
typedef enum
{
    SGP30_EVENT_NEW_MEASUREMENT, /*!< New measurement available */
    SGP30_EVENT_NEW_BASELINE,   /*!< New baseline available */
} sgp30_event_id_t;

/**
 * @brief SGP30 event handler registration.
 * This structure is used to register event handlers for the SGP30 module.
 */
typedef struct
{
    sgp30_event_id_t event_id; /*!< Event ID to register the handler for */
    esp_event_handler_t event_handler; /*!< Event handler function */
} sgp30_event_handler_register_t;

/**
 * @brief SGP30 registers for read/write operations.
 */
typedef enum
{
    SGP30_REG_INIT_AIR_QUALITY = 0x2003,        /*!< Init Air Quality */
    SGP30_REG_MEASURE_AIR_QUALITY = 0x2008,     /*!< Measure Air Quality */
    SGP30_REG_GET_BASELINE = 0x2015,            /*!< Get Baseline */
    SGP30_REG_SET_BASELINE = 0x201e,            /*!< Set Baseline */
    SGP30_REG_MEASURE_TEST = 0x2032,            /*!< Measure Test */
    SGP30_REG_GET_FEATURE_SET_VERSION = 0x202f, /*!< Get Feature Set Version */
    SGP30_REG_MEASURE_RAW_SIGNALS = 0x2050,     /*!< Measure Raw Signals */
    SGP30_REG_SET_HUMIDITY = 0x2061,            /*!< Set Humidity */
    SGP30_REG_GET_SERIAL_ID = 0x3682,           /*!< Get Serial ID */
} sgp30_register_rw_t;

ESP_EVENT_DECLARE_BASE(SGP30_EVENT);

/**
 * @brief Checks if the baseline is expired.
 *
 * @param stored time_t value of the stored baseline
 * @param current time_t value of the current time
 * @return
 *   - true : if the baseline is expired
 *   - false : otherwise
 */
bool sgp30_is_baseline_expired(time_t stored, time_t current);

/**
 * @brief Gets the mean of a measurement log.
 * This function calculates the mean of the measurements in the log.
 * @param m Pointer to the measurement structure where the mean will be stored.
 * @param q Pointer to the measurement log.
 * @return
 *  - ESP_OK : if the mean was calculated successfully
 */
esp_err_t sgp30_measurement_log_get_mean(
    sgp30_measurement_t *m,
    const sgp30_measurement_log_t *q
);

/**
 * @brief Enqueues a measurement in the log.
 * This function enqueues a measurement in the log.
 * @param m Pointer to the measurement structure to be enqueued.
 * @param q Pointer to the measurement log.
 * @return
 *  - ESP_OK : if the measurement was enqueued successfully
 */
esp_err_t sgp30_measurement_log_enqueue(
    const sgp30_measurement_t *m,
    sgp30_measurement_log_t *q
);
/**
 * @brief Dequeues a measurement from the log.
 * This function dequeues a measurement from the log.
 * @param m Pointer to the measurement structure where the dequeued measurement
 * will be stored.
 * @param q Pointer to the measurement log.
 * @return
 *  - ESP_OK : if the measurement was dequeued successfully
 */
esp_err_t sgp30_measurement_log_dequeue(
    sgp30_measurement_t *m,
    sgp30_measurement_log_t *q
);

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
    const uint32_t dev_speed
);
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
 * @brief Initializes the SGP30 device.
 *
 * This function initializes all structures needed for the SGP30 device to
 * function properly. It also sets the baseline value if provided.
 *
 * @param loop The event loop where the SGP30 events will be posted.
 * @param baseline Pointer to the baseline value to be set.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Failed to initialize the device
 */
esp_err_t
sgp30_init(esp_event_loop_handle_t loop, sgp30_measurement_t *baseline);
/**
 * @brief Start publishing measurements from the SGP30 sensor.
 * This function sets the sgp30 to begin publishing measurements on the specified module event loop.
 * @param s The interval in seconds at which the measurements will be published.
 * @return
 *  - ESP_OK : if the timer was started successfully
 *  - ESP_FAIL : if the timer could not be started
 */
esp_err_t sgp30_start_measuring(uint64_t s);
/**
 * @brief Restarts the measurement timer with a new interval.
 * This function restarts the measurement timer with a new interval.
 * @param new_measurement_interval New interval for the measurement timer in seconds.
 * @return
 *   - ESP_OK : if the timer was restarted successfully
 *   - ESP_FAIL : if the timer could not be restarted
 */
esp_err_t sgp30_restart_measuring(uint64_t new_measurement_interval);
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
 * This function communicates with the SGP30 sensor over I2C to obtain the
 * current eCO2 and TVOC measurements. Then posts a
 * SENSOR_EVENT_NEW_MEASUREMENT to the sgp30_event_loop
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_FAIL: Communication with the sensor failed
 *     - ESP_ERR_INVALID_CRC: Received wrong chechsum
 */
esp_err_t sgp30_measure_air_quality(
    i2c_master_dev_handle_t dev_handle,
    sgp30_measurement_t *new_measurement
);
/**
 * @brief Retrieve the baseline from the SGP30 sensor.
 *
 * This function communicates with the SGP30 sensor over I2C to obtain the
 * current baseline. Then posts a SENSOR_EVENT_NEW_BASELINE to the
 * sgp30_event_loop
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_FAIL: Communication with the sensor failed
 *     - ESP_ERR_INVALID_CRC: Received wrong chechsum
 */
esp_err_t sgp30_get_baseline(
    i2c_master_dev_handle_t dev_handle,
    sgp30_measurement_t *baseline
);
/**
 * @brief Set the baseline in the SGP30 sensor.
 *
 * This function communicates with the SGP30 sensor over I2C to set the
 * baseline to the provided value.
 *
 * @param dev_handle Handle to the I2C master device.
 * @param baseline Pointer to the baseline value to be set.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Communication with the sensor failed
 *     - ESP_ERR_INVALID_CRC: Received wrong chechsum
 */
esp_err_t sgp30_set_baseline(
    i2c_master_dev_handle_t dev_hanlde,
    const sgp30_measurement_t *baseline
);

esp_err_t sgp30_measure_air_quality_and_post_esp_event();
esp_err_t sgp30_get_baseline_and_post_esp_event();
esp_err_t sgp30_set_baseline_and_post_esp_event();
/**
 * @brief Retrieve the ID from the SGP30 sensor.
 *
 * This function communicates with the SGP30 sensor over I2C to obtain its
 * unique identifier.
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
