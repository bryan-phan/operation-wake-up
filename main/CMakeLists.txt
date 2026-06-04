idf_component_register(SRCS "main.c" "drivers/lcd/lcd.c"
                    INCLUDE_DIRS "." "drivers/lcd"
                    REQUIRES esp_driver_i2c)
