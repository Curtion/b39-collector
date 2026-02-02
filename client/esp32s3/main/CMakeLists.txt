idf_component_register(SRCS "main.c"
                            "wifi_manager.c"
                            "http_client.c"
                            "http_server.c"
                            "usb_cdc.c"
                            "gpio_button.c"
                            "ws2812b.c"
                            "led_status.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES usb nvs_flash esp_wifi esp_http_client esp_http_server esp_driver_gpio esp_driver_rmt spiffs fatfs json vfs
                       )

spiffs_create_partition_image(storage ${CMAKE_CURRENT_SOURCE_DIR}/web FLASH_IN_PROJECT)
