#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include <string.h>
#include "nvs_component.h"
#include "esp_log.h"
#define STORAGE_NAMESPACE "storage"
static const char *TAG = "NVS_COMPONENT";
esp_err_t set_version(char *value)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    int required_size = strlen(value);
    char *version = malloc(required_size * sizeof(char));

    strcpy(version, value);
    err = nvs_set_str(my_handle, "version", version);
    free(version);

    if (err != ESP_OK)
        return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
        return err;

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

char *get_version()
{
    char *version = NULL;
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle));

    // Read run time blob
    size_t required_size = 0; // value will default to 0, if not set yet in NVS
    // obtain required memory space to store blob being read from NVS
    err = nvs_get_str(my_handle, "version", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        ESP_ERROR_CHECK(err);

    if (required_size == 0)
    {
        printf("Nothing saved yet!\n");
    }
    else
    {
        version = malloc(required_size);
        err = nvs_get_str(my_handle, "version", version, &required_size);
        if (err != ESP_OK)
        {
            free(version);
            ESP_ERROR_CHECK(err);
        }
        ESP_LOGI(TAG, "OTA VERSION %s:\n", version);
    }

    // Close
    nvs_close(my_handle);
    return version;
}

uint8_t get_compared_version(char *version)
{
    char *versionRN = NULL;
    versionRN = get_version();
    if (!versionRN)
        return 1;
    int result = strcmp(versionRN, version);
    free(versionRN);
    return result == 0 ? 0 : 1;
}
