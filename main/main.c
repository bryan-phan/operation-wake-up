#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "driver/i2c_master.h"
#include "rom/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "drivers/i2c_bus/i2c_bus.h"
#include "lcd.h"
#include "secrets.h"


static i2c_master_dev_handle_t rtc_handle;

#define RTC_ADDR        0x68
#define RTC_SPEED_HZ    100000


#define SECS        0x00
#define MINS        0x01
#define HOURS       0x02
#define DAY         0x03
#define DATE        0x04
#define MONTH       0x05
#define YEAR        0x06
#define CONTROL     0x07

// RTC is in reference of the DS1307 RTC device

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t date;
    uint8_t month;
    uint8_t year;
} rtc_time_t;

void ds1307_init(void){
    if (rtc_handle == NULL) {
    i2c_device_config_t rtc_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = RTC_ADDR,
        .scl_speed_hz = RTC_SPEED_HZ,
    };

    ESP_ERROR_CHECK(i2c_bus_add_device(&rtc_cfg, &rtc_handle));
}
}

uint8_t bcd2dec (uint8_t bcd){
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

uint8_t dec2bcd (uint8_t dec){
    return ((dec / 10) << 4 | (dec % 10));
}

static uint8_t rtc_decode_hours(uint8_t hour_reg){
    if (hour_reg & 0x40) {
        uint8_t hour12 = bcd2dec(hour_reg & 0x1F);
        bool is_pm = (hour_reg & 0x20) != 0;

        if (hour12 == 12) {
            return is_pm ? 12 : 0;
        }

        return is_pm ? hour12 + 12 : hour12;
    }

    return bcd2dec(hour_reg & 0x3F);
}

esp_err_t rtc_read_time(rtc_time_t *time){

    uint8_t reg = SECS;
    uint8_t data[7];

    // Read next 7 bytes starting at 0x00
    esp_err_t err = i2c_master_transmit_receive(rtc_handle, &reg, 1, data, 7, -1);
    if (err != ESP_OK) return err;

    time->seconds = bcd2dec(data[0] & 0x7F);
    time->minutes = bcd2dec(data[1] & 0x7F);
    time->hours = rtc_decode_hours(data[2]);
    time->day = data[3] & 0x07;
    time->date = bcd2dec(data[4] & 0x3F);
    time->month = bcd2dec(data[5] & 0x1F);
    time->year = bcd2dec(data[6]);

    return ESP_OK;
}

esp_err_t rtc_write_time(const rtc_time_t *time){
    uint8_t buf[8];
    
    buf[0] = SECS;
    buf[1] = dec2bcd(time->seconds) & 0x7F;
    buf[2] = dec2bcd(time->minutes) & 0x7F;
    buf[3] = dec2bcd(time->hours) & 0x3F;
    buf[4] = time->day & 0x07;
    buf[5] = dec2bcd(time->date) & 0x3F;
    buf[6] = dec2bcd(time->month) & 0x1F;
    buf[7] = dec2bcd(time->year);

    return i2c_master_transmit(rtc_handle, buf, sizeof(buf), -1);
}

static void get_time(const rtc_time_t *time, char *time_buf, size_t buf_size){
    uint8_t display_hour = time->hours % 12;
    if (display_hour == 0) {
        display_hour = 12;
    }

    const char *ampm = (time->hours < 12) ? "AM" : "PM";

    snprintf(time_buf, buf_size, "%02d:%02d:%02d %s",
             display_hour, time->minutes, time->seconds, ampm);
}

static void get_date(const rtc_time_t *time, char *date_buf, size_t buf_size){
    snprintf(date_buf, buf_size, "%02d/%02d/20%02d",
             time->month, time->date, time->year);
}

#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t wifi_events;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_events, WIFI_CONNECTED_BIT);
    }
}

static void wifi_sync_rtc(void) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    wifi_config_t wifi_config = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS } };
    wifi_events = xEventGroupCreate();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
    xEventGroupWaitBits(wifi_events, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
    tzset();

    struct tm timeinfo = {0};
    while (timeinfo.tm_year < (2020 - 1900)) {
        vTaskDelay(pdMS_TO_TICKS(500));
        time_t now;
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    rtc_time_t t = {
        .seconds = timeinfo.tm_sec,
        .minutes = timeinfo.tm_min,
        .hours   = timeinfo.tm_hour,
        .day     = timeinfo.tm_wday + 1,
        .date    = timeinfo.tm_mday,
        .month   = timeinfo.tm_mon + 1,
        .year    = timeinfo.tm_year - 100,
    };
    ESP_ERROR_CHECK(rtc_write_time(&t));

    esp_wifi_stop();
    esp_wifi_deinit();
}

void app_main(void){
    ESP_ERROR_CHECK(i2c_bus_init());
    lcd_init();
    ds1307_init();

    ESP_ERROR_CHECK(lcd_print_line(0, "Syncing time..."));
    wifi_sync_rtc();

    rtc_time_t time;

    while (true){
        char date_buf[16];
        char time_buf[16];

        ESP_ERROR_CHECK(rtc_read_time(&time));
        get_date(&time, date_buf, sizeof(date_buf));
        get_time(&time, time_buf, sizeof(time_buf));

        ESP_ERROR_CHECK(lcd_print_line(0, date_buf));
        ESP_ERROR_CHECK(lcd_print_line(1, time_buf));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
