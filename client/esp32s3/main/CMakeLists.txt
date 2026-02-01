idf_component_register(SRCS "main.c"
                            "wifi_manager.c"
                            "http_client.c"
                            "usb_cdc.c"
                            "gpio_button.c"
                            "ws2812b.c"
                            "led_status.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES usb nvs_flash esp_wifi esp_http_client driver esp_driver_rmt
                       )
