set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS};C:/Users/jaime/esp-idf/examples/common_components/protocol_examples_common")

idf_component_register(SRCS "src/ota.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_event esp_netif esp_wifi esp-tls esp_http_client protocol_examples_common nvs_flash nvs_component json app_update esp_https_ota)
