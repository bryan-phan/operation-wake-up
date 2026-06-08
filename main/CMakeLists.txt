idf_component_register(SRCS "main.c" "drivers/wifi/app_wifi.c" "drivers/lcd/lcd.c" "drivers/i2c_bus/i2c_bus.c" "drivers/rtc/rtc.c"
                    INCLUDE_DIRS "." "drivers/wifi/include" "drivers/lcd" "drivers/i2c_bus" "drivers/rtc"
                    REQUIRES esp_driver_i2c esp_event esp_netif esp_wifi nvs_flash
                    )
