idf_component_register(SRCS "main.c"
                            "wifi_manager.c"
                            "http_client.c"
                            "usb_cdc.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES usb nvs_flash esp_wifi esp_http_client
                       )
