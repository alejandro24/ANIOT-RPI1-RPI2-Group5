# This defines the component directory
idf_component_register(SRCS "src/sntp_sync.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_wifi esp_event esp_system lwip nvs_flash)
