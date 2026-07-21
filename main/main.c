#include <stdio.h>
#include "esp_err.h"
#include "net.h"
#include "sd-card.h"
#include "app_wifi.h"
#include "esp_log.h"
#include "player.h"
#include "esp_system.h"
#include "audio.h"
#include "mp3.h"
#include <sys/stat.h>
#include "i2c_bus.h"
#include "rtc.h"
#include "lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include "driver/gpio.h"
#include "iot_button.h"
#include "button_gpio.h"


#define URL "https://pub-b88a0936b6534894a6fbae56fbc30119.r2.dev/Mariah%20Carey%20-%20We%20Belong%20Together%20(Official%20Music%20Video).mp3"
static const char *TAG = "mp3-download";
static const char *TAG2 = "button";

typedef struct alarm_time_t {
    uint8_t hours;
    uint8_t minutes;
    bool alarm_bool;
} alarm_time_t;

typedef enum{ 
    MODE_RUN, 
    MODE_SET_CLOCK, 
    MODE_SET_ALARM
} clock_mode_t;

static volatile clock_mode_t current_mode = MODE_RUN;

static void write_chunk(const char *data, size_t len, void *ctx)
{
    FILE *f = (FILE *)ctx;
    fwrite(data, 1, len, f);
}

static void change_mode_cb(void *arg,void *usr_data)
{
    current_mode = (current_mode + 1) % 3;

    switch(current_mode) {
        case MODE_RUN:
            ESP_LOGI(TAG2, "Mode: Run");
            break;
        case MODE_SET_CLOCK:
            ESP_LOGI(TAG2, "Mode: Set Clock");
            break;
        case MODE_SET_ALARM:
            ESP_LOGI(TAG2, "Mode: Set Alarm");
            break;
    }
}

static void snooze_cb(void *arg,void *usr_data)
{
    switch(current_mode) {
        case MODE_RUN:
            ESP_LOGI(TAG2, "Snooze button pressed");
            player_stop();
            break;
        case MODE_SET_CLOCK:
            ESP_LOGI(TAG2, "Snooze button pressed in Set Clock mode");
            
            break;
        case MODE_SET_ALARM:
            ESP_LOGI(TAG2, "Snooze button pressed in Set Alarm mode");
            break;
    }
}

void app_main(void)
{

    ds1307_init();
    lcd_init();

    // create gpio button
    const button_config_t mode_cfg = {0};
    const button_gpio_config_t mode_gpio_cfg = {
        .gpio_num = 17,
        .active_level = 0,
    };
    button_handle_t mode_btn = NULL;
    esp_err_t ret = iot_button_new_gpio_device(&mode_cfg, &mode_gpio_cfg, &mode_btn);
    if(NULL == mode_btn) {
        ESP_LOGE(TAG2, "Button create failed");
    }

    // create gpio button
    const button_config_t snooze_cfg = {0};
    const button_gpio_config_t snooze_gpio_cfg = {
        .gpio_num = 16,
        .active_level = 0,
    };
    button_handle_t snooze_btn = NULL;
    ret = iot_button_new_gpio_device(&snooze_cfg, &snooze_gpio_cfg, &snooze_btn);
    if(NULL == snooze_btn) {
        ESP_LOGE(TAG2, "Button create failed");
    }

    // create gpio button
    const button_config_t alarm_off_cfg = {0};
    const button_gpio_config_t alarm_off_gpio_cfg = {
        .gpio_num = 32,
        .active_level = 0,
    };
    button_handle_t alarm_off_btn = NULL;
    ret = iot_button_new_gpio_device(&alarm_off_cfg, &alarm_off_gpio_cfg, &alarm_off_btn);
    if(NULL == alarm_off_btn) {
        ESP_LOGE(TAG2, "Button create failed");
    }

    
    iot_button_register_cb(mode_btn, BUTTON_SINGLE_CLICK, NULL, change_mode_cb,NULL);
    //iot_button_register_cb(snooze_btn, BUTTON_SINGLE_CLICK, NULL, button_single_click_cb,NULL);
    //iot_button_register_cb(alarm_off_btn, BUTTON_SINGLE_CLICK, NULL, button_single_click_cb,NULL);


    struct stat st;

    esp_err_t ret;
    ESP_ERROR_CHECK(sdc_init());
    ESP_ERROR_CHECK(i2c_bus_init());
    ds1307_init();
    lcd_init();

    if (stat("/sdcard/Mariah.mp3", &st) != 0) {
        ESP_ERROR_CHECK(app_wifi_connect());
        FILE *f = fopen("/sdcard/Mariah.mp3", "wb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            return;
        }

        ret = net_https_get(URL, write_chunk, f);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to download file");
            return;
        }
        fclose(f);
    }

    rtc_time_t time;
    alarm_time_t alarm;
    char time_buf[16];

    alarm.hours = 12;
    alarm.minutes = 3;
    alarm.alarm_bool = true;   

    bool alarm_fired = false;

    while (1) {
        ESP_ERROR_CHECK(rtc_read_time(&time));

        bool match = alarm.alarm_bool &&
                     time.hours == alarm.hours &&
                     time.minutes == alarm.minutes;

        get_time(&time, time_buf, sizeof(time_buf));

        ESP_LOGI(TAG, "Current time: %s", time_buf);


        lcd_clear();
        lcd_print_line(0, time_buf);

        
        if (match && !alarm_fired){
            ESP_LOGI(TAG, "Playing the downloaded file");
            player_play("/sdcard/Mariah.mp3");
            alarm_fired = true;
        }
        if (!match) {
            alarm_fired = false;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));

    }

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

}
