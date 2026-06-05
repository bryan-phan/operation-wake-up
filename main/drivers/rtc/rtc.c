#include <stdio.h>
#include "driver/i2c_master.h"
#include "drivers/i2c_bus/i2c_bus.h"
#include "rtc.h"

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

// Convert BCD to DEC and vice versa
uint8_t bcd2dec (uint8_t bcd){
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

uint8_t dec2bcd (uint8_t dec){
    return ((dec / 10) << 4 | (dec % 10));
}

esp_err_t rtc_read_time(rtc_time_t *time){

    uint8_t reg = SECS;
    uint8_t data[7];

    // Read next 7 bytes starting at 0x00
    esp_err_t err = i2c_master_transmit_receive(rtc_handle, &reg, 1, data, 7, -1);
    if (err != ESP_OK) return err;

    time->seconds = bcd2dec(data[0] & 0x7F);
    time->minutes = bcd2dec(data[1] & 0x7F);
    time->hours = bcd2dec(data[2] & 0x3F);
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

void format_time(const rtc_time_t *time, char *time_buf, size_t buf_size){
    uint8_t display_hour = time->hours % 12;
    if (display_hour == 0) {
        display_hour = 12;
    }

    const char *ampm = (time->hours < 12) ? "AM" : "PM";

    snprintf(time_buf, buf_size, "%02d:%02d:%02d %s",
             display_hour, time->minutes, time->seconds, ampm);
}

void format_date(const rtc_time_t *time, char *date_buf, size_t buf_size){
    snprintf(date_buf, buf_size, "%02d/%02d/20%02d",
             time->month, time->date, time->year);
}
