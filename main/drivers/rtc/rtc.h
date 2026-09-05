#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

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

void ds1307_init(void);

// Convert BCD to DEC and vice versa
uint8_t bcd2dec(uint8_t bcd);
uint8_t dec2bcd(uint8_t dec);

esp_err_t rtc_read_time(rtc_time_t *time);
esp_err_t rtc_write_time(const rtc_time_t *time);

void get_time(const rtc_time_t *time, char *time_buf, size_t buf_size);
void get_date(const rtc_time_t *time, char *date_buf, size_t buf_size);
