idf_component_register(SRCS "nvs_structures.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_timer nvs_flash sgp30 nvs_structures softAP_provision)
