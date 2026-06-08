#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "i2c_bus.h"
#include "lcd.h"
#include "rtc.h"

static const char *TAG = "main";

void app_main(void)
{
    rtc_time_t now = {0};
    char time_buf[17];
    char date_buf[17];

    ESP_ERROR_CHECK(i2c_bus_init());
    ds1307_init();
    lcd_init();

    while (true) {
        esp_err_t err = rtc_read_time(&now);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "rtc_read_time failed: %s", esp_err_to_name(err));
            ESP_ERROR_CHECK(lcd_print_line(0, "RTC read failed"));
            ESP_ERROR_CHECK(lcd_print_line(1, "Check wiring"));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        get_date(&now, date_buf, sizeof(date_buf));
        get_time(&now, time_buf, sizeof(time_buf));

        ESP_ERROR_CHECK(lcd_print_line(0, date_buf));
        ESP_ERROR_CHECK(lcd_print_line(1, time_buf));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
