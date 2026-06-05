#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "drivers/i2c_bus/i2c_bus.h"
#include "lcd.h"
#include "rtc.h"

void app_main(void){
    ESP_ERROR_CHECK(i2c_bus_init());
    lcd_init();
    ds1307_init();

    rtc_time_t time;

    while (true){
        char date_buf[16];
        char time_buf[16];

        ESP_ERROR_CHECK(rtc_read_time(&time));
        format_date(&time, date_buf, sizeof(date_buf));
        format_time(&time, time_buf, sizeof(time_buf));

        ESP_ERROR_CHECK(lcd_print_line(0, date_buf));
        ESP_ERROR_CHECK(lcd_print_line(1, time_buf));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
