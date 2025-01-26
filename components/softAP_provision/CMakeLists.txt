idf_component_register(SRCS "softAP_provision.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi wifi_provisioning esp_event mqtt_controller json)
