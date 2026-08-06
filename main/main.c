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
#define SONG_PATH "/sdcard/Mariah.mp3"

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

rtc_time_t now_time;
alarm_time_t alarm;
static volatile clock_mode_t current_mode = MODE_RUN;

static void write_chunk(const char *data, size_t len, void *ctx)
{
    FILE *f = (FILE *)ctx;
    fwrite(data, 1, len, f);
}

// Create one active-low GPIO button and hook its single click to `cb`.
static button_handle_t make_button(int gpio_num, button_cb_t cb)
{
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t gpio_cfg = {
        .gpio_num = gpio_num,
        .active_level = 0,
    };

    button_handle_t btn = NULL;
    if (iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn) != ESP_OK || btn == NULL) {
        ESP_LOGE(TAG2, "Button create failed on GPIO %d", gpio_num);
        return NULL;
    }

    iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, cb, NULL);
    return btn;
}

// Download the song to the SD card the first time (skips if it's already there).
static void download_song_if_needed(void)
{
    struct stat st;
    if (stat(SONG_PATH, &st) == 0) {
        return;                       // already downloaded
    }

    ESP_ERROR_CHECK(app_wifi_connect());
    FILE *f = fopen(SONG_PATH, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    if (net_https_get(URL, write_chunk, f) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to download file");
    }
    fclose(f);
}

static void change_mode_cb(void *arg, void *usr_data)
{
    // Leaving set-clock: save the edited time to the RTC chip.
    if (current_mode == MODE_SET_CLOCK) {
        rtc_write_time(&now_time);
    }

    current_mode = (current_mode + 1) % 3;

    switch (current_mode) {
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

static void snooze_cb(void *arg, void *usr_data)
{
    switch (current_mode) {
        case MODE_RUN:
            ESP_LOGI(TAG2, "Snooze button pressed in Run mode");
            player_stop();
            break;
        case MODE_SET_CLOCK:
            ESP_LOGI(TAG2, "Snooze button pressed in Set Clock mode");
            now_time.hours = (now_time.hours + 1) % 24;
            break;
        case MODE_SET_ALARM:
            ESP_LOGI(TAG2, "Snooze button pressed in Set Alarm mode");
            alarm.hours = (alarm.hours + 1) % 24;
            break;
    }
}

static void alarm_off_cb(void *arg, void *usr_data)
{
    switch (current_mode) {
        case MODE_RUN:
            ESP_LOGI(TAG2, "Alarm Off button pressed in Run mode");
            player_stop();
            break;
        case MODE_SET_CLOCK:
            ESP_LOGI(TAG2, "Alarm Off button pressed in Set Clock mode");
            now_time.minutes = (now_time.minutes + 1) % 60;
            break;
        case MODE_SET_ALARM:
            ESP_LOGI(TAG2, "Alarm Off button pressed in Set Alarm mode");
            alarm.minutes = (alarm.minutes + 1) % 60;
            break;
    }
}

// Create all three buttons and wire up their callbacks.
static void init_buttons(void)
{
    make_button(17, change_mode_cb);   // MODE
    make_button(16, snooze_cb);        // SNOOZE  / hours +
    make_button(32, alarm_off_cb);     // ALARM OFF / minutes +
}

void app_main(void)
{
    ESP_ERROR_CHECK(sdc_init());
    ESP_ERROR_CHECK(i2c_bus_init());
    ds1307_init();
    lcd_init();

    download_song_if_needed();
    init_buttons();

    alarm.hours = 2;
    alarm.minutes = 5;
    alarm.alarm_bool = true;

    char time_buf[16];
    bool alarm_fired = false;

    while (1) {
        // Only read the chip in RUN. In set modes we leave now_time alone so
        // the button edits survive, and we run the alarm check only when live.
        if (current_mode == MODE_RUN) {
            ESP_ERROR_CHECK(rtc_read_time(&now_time));

            bool match = alarm.alarm_bool &&
                         now_time.hours == alarm.hours &&
                         now_time.minutes == alarm.minutes;

            if (match && !alarm_fired) {
                ESP_LOGI(TAG, "Playing the downloaded file");
                player_play(SONG_PATH);
                alarm_fired = true;
            }
            if (!match) {
                alarm_fired = false;
            }
        }

        get_time(&now_time, time_buf, sizeof(time_buf));
        ESP_LOGI(TAG2, "Current time: %s", time_buf);

        lcd_clear();
        lcd_print_line(0, time_buf);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
