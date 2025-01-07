message("Include directories: ${INCLUDE_DIRS}")
idf_component_register(SRCS "mqtt.c"
                       INCLUDE_DIRS "include"
                       REQUIRES mqtt esp_partition nvs_flash esp_netif app_update json)