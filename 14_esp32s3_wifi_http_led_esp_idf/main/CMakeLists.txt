idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES 
                        nvs_flash 
                        esp_wifi 
                        esp_event
                        esp_https_server
                        esp_driver_gpio
                        esp_netif
                        lwip
                        driver
                    )
