# wifi_manager/CMakeLists.txt

idf_component_register(SRCS "mqtt.c" "src/mqtt.c"
                       INCLUDE_DIRS "include"
                       REQUIRES  )
