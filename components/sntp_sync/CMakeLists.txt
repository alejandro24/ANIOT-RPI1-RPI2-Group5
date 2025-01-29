idf_component_register(
    SRCS "sntp_sync.c"  # Reemplázalo con el nombre real del archivo fuente
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_wifi
)