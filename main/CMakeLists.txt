idf_component_register(SRCS "main.c" "drivers/lcd/lcd.c" "drivers/i2c_bus/i2c_bus.c"
                    INCLUDE_DIRS "." "drivers/lcd" "drivers/i2c_bus"
                    REQUIRES esp_driver_i2c esp_wifi esp_event esp_netif nvs_flash esp_timer
                    )
