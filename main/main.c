#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "drivers/lcd/lcd.h"

void app_main(void){
    i2c_init();
    lcd_init();

    while (true){
        ESP_ERROR_CHECK(lcd_print_line(0, "Am I the goat?"));
        ESP_ERROR_CHECK(lcd_print_line(1, "Maybe not..."));

        vTaskDelay(pdMS_TO_TICKS(1500));

        ESP_ERROR_CHECK(lcd_clear());
        ets_delay_us(200);

        for (int i = 0; i < 100; i++){
            char buf[16];

            ESP_ERROR_CHECK(lcd_set_cursor(7, 0));
            snprintf(buf, sizeof(buf), "%d", i);
            ESP_ERROR_CHECK(lcd_print(buf));
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        ESP_ERROR_CHECK(lcd_scroll_line(0, "IM SO HUNGRY", 300));
        ESP_ERROR_CHECK(lcd_scroll_line(1, "I need to sleep soon...", 300));

        ESP_ERROR_CHECK(lcd_clear());
        ets_delay_us(200);
    }
}
