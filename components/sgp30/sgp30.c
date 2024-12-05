#include "esp_check.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_event_base.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
#include "sgp30.h"

ESP_EVENT_DEFINE_BASE(SENSOR_EVENTS);

static char* TAG = "SGP30";
// This is a very inefficient
static esp_err_t crc8_check(uint8_t *buffer, size_t size) {
    uint8_t crc = 0;

    for (int i = 0; i < size; i++) {
        // We XOR the first/next bit into the crc
        crc ^= buffer[i];
        for (int j = 0; j < 8; j++) {
            // Check if the MSB is set to 1
            if (crc & 0x80) {
                // If it is set to 1 get rid of the 1 and XOR
                crc = (crc << 1) ^ SGP30_CRC_8_POLY;
            } else {
                // If it is not, just keep looking for a 1
                crc <<= 1;
            }
        }
    }

    // Error if reminder is not zero.
    if (crc) {
        return ESP_ERR_INVALID_CRC;
    } else {
        return ESP_OK;
    }
}

static uint8_t crc8_gen(uint8_t *buffer, size_t size) {
    uint8_t crc = 0;

    for (int i = 0; i < size; i++) {
        crc ^= buffer[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ SGP30_CRC_8_POLY;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

i2c_master_dev_handle_t sgp30_device_create(
    i2c_master_bus_handle_t bus_handle,
    const uint16_t dev_addr,
    const uint32_t dev_speed)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = dev_speed,
    };

    i2c_master_dev_handle_t dev_handle;

    // Add device to the I2C bus
    ESP_ERROR_CHECK(
        i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle)
    );

    return dev_handle;
}

esp_err_t sgp30_device_delete(i2c_master_dev_handle_t dev_handle)
{
    return i2c_master_bus_rm_device(dev_handle);
}

esp_err_t sgp30_init_air_quality(i2c_master_dev_handle_t dev_handle)
{
    sgp30_register_rw_t write_addr = SGP30_REG_INIT_AIR_QUALITY;
    uint8_t write_buffer[2];
    write_buffer[0] = write_addr & 0xFF; /* Mask should be unnecessary */
    write_buffer[1] = (write_addr >> 8) & 0xFF; /* Mask should be unnecessary */

    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(dev_handle, write_buffer, 2, -1),
        TAG, "I2C init air quality failed"
    );

    // The sensor needs a max of 10 ms to respond to the I2C read header.
    vTaskDelay(pdMS_TO_TICKS(1));

    return ESP_OK;
}

esp_err_t sgp30_measure_air_quality(i2c_master_dev_handle_t dev_handle, uint16_t *eCO2, uint16_t *TVOC)
{
    sgp30_register_rw_t write_addr = SGP30_REG_MEASURE_AIR_QUALITY;
    uint8_t write_buffer[2];
    write_buffer[0] = write_addr & 0xFF; /* Mask should be unnecessary */
    write_buffer[1] = (write_addr >> 8) & 0xFF; /* Mask should be unnecessary */

    /* We return 2 words (16 bits/w) each followed by an 8 bit CRC checksum */
    /* Therefore we need to save 48 bits for the received data */
    uint8_t bytes_read[6];

    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(dev_handle, write_buffer, 2, -1),
        TAG, "I2C get serial id write failed"
    );

    // The sensor needs a max of 12 ms to respond to the I2C read header.
    vTaskDelay(pdMS_TO_TICKS(12));

    ESP_RETURN_ON_ERROR(
        i2c_master_receive(dev_handle, bytes_read, 6, -1),
        TAG, "I2C get serial id read failed"
    );

    ESP_RETURN_ON_ERROR(
        crc8_check(bytes_read, 6),
        TAG, "I2C get serial id CRC failed"
    );

    // Copy the read ID to the provided id pointer
    *eCO2 = (uint16_t) (bytes_read[1] << 8) | bytes_read[0];
    // bytes_read[2] is CRC for the first word, we can ignore it.

    *TVOC = (uint16_t) (bytes_read[4] << 8) | bytes_read[3];
    // bytes_read[5] is CRC for the second word, we can ignore it.
    return ESP_OK;
}

esp_err_t sgp30_get_id(i2c_master_dev_handle_t dev_handle, uint8_t *id)
{
    sgp30_register_rw_t write_addr = SGP30_REG_GET_SERIAL_ID;
    uint8_t write_buffer[2];
    write_buffer[0] = write_addr & 0xFF; /* Mask should be unnecessary */
    write_buffer[1] = (write_addr >> 8) & 0xFF; /* Mask should be unnecessary */

    /* We return 3 words (16 bits/w) each followed by an 8 bit CRC checksum */
    /* Therefore we need to save 72 bits for the received data */
    uint8_t bytes_read[9];

    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(dev_handle, write_buffer, 2, -1),
        TAG, "I2C get serial id write failed"
    );

    // The sensor needs .5 ms to respond to the I2C read header.
    vTaskDelay(pdMS_TO_TICKS(1));

    ESP_RETURN_ON_ERROR(
        i2c_master_receive(dev_handle, bytes_read, 9, -1),
        TAG, "I2C get serial id read failed"
    );

    ESP_RETURN_ON_ERROR(
        crc8_check(bytes_read, 9),
        TAG, "I2C get serial id CRC failed"
    );

    // Copy the read ID to the provided id pointer
    id[0] = bytes_read[0];
    id[1] = bytes_read[1];
    // bytes_read[2] is CRC for the first word, we can ignore it.

    id[2] = bytes_read[3];
    id[3] = bytes_read[4];
    // bytes_read[5] is CRC for the second word, we can ignore it.

    id[4] = bytes_read[6];
    id[5] = bytes_read[7];
    // bytes_read[8] is CRC for the third word, we can ignore it.

    return ESP_OK;
}
