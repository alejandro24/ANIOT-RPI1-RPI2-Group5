#include "esp_err.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_event_base.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sgp30.h"

ESP_EVENT_DEFINE_BASE(SENSOR_EVENTS);

static char* TAG = "SGP30";

static esp_err_t crc8_check(uint8_t *buffer, size_t size) {
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
}

i2c_master_dev_handle_t shtc3_device_create(
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
    );:

    return dev_handle;
}

esp_err_t shtc3_device_delete(i2c_master_dev_handle_t dev_handle)
{
    return i2c_master_bus_rm_device(dev_handle);
}

esp_err_t shtc3_get_id(i2c_master_dev_handle_t dev_handle, uint8_t *id)
{
    shtc3_register_rw_t write_addr = SGP30_REG_READ_ID;
    /* We return 3 words (16 bits/w) each followed by an 8 bit CRC checksum */
    /* Therefore we need to save 72 bits for the received data */
    uint8_t bytes_read[9];

    ESP_RETURN_ON_ERROR(i2c_master_transmit(dev_handle, write_addr, 2, -1));

    // The sensor needs .5 ms to respond to the I2C read header.
    xTaskDelay(pdMS_TO_TICKS(1));

    ESP_RETURN_ON_ERROR(i2c_master_receive(dev_handle, read_reg, 9, -1));

    ESP_RETURN_ON_ERROR(crc8_check(bytes_read, 9));

    // Copy the read ID to the provided id pointer
    id[0] = bytes_read[0];
    id[1] = bytes_read[1];
    // bytes_read[2] is CRC for the first wor, we can ignore it.
    id[2] = bytes_read[3];
    id[3] = bytes_read[4];
    // bytes_read[5] is CRC for the second wor, we can ignore it.
    id[4] = bytes_read[6];
    id[5] = bytes_read[7];
    // bytes_read[8] is CRC for the third word, we can ignore it.

    return ret;
}
