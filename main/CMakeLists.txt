idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver freertos esp_wifi nvs_flash esp_event esp_http_server)
