idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_event esp_http_server esp_lcd esp_netif esp_timer esp_wifi lwip nvs_flash)
