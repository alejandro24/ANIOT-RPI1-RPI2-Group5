# wifi_manager/CMakeLists.txt

idf_component_register(SRCS "src/wifi.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_wifi nvs_flash)
