idf_component_register(SRCS "sgp30.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_timer esp_driver_i2c nvs_flash nvs_structures
    PRIV_REQUIRES esp_driver_i2c)
