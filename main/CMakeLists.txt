idf_component_register(SRCS "main.c" "drivers/lcd/lcd.c" "drivers/i2c_bus/i2c_bus.c" "drivers/rtc/rtc.c"
                    INCLUDE_DIRS "." "drivers/lcd" "drivers/i2c_bus" "drivers/rtc"
                    REQUIRES esp_driver_i2c
                    )
