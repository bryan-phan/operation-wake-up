#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

void i2c_init(void);
esp_err_t HLF8574_write(uint8_t data);
esp_err_t lcd_pulse_enable(uint8_t data);
esp_err_t lcd_write_nibble(uint8_t nibble, uint8_t rs);
esp_err_t lcd_send(uint8_t byte, uint8_t rs);
esp_err_t lcd_cmd(uint8_t cmd);
esp_err_t lcd_char(char c);
void lcd_init(void);
esp_err_t lcd_set_cursor(uint8_t col, uint8_t row);
esp_err_t lcd_print_line(uint8_t row, const char *str);
esp_err_t lcd_print(const char *str);
esp_err_t lcd_clear(void);
esp_err_t lcd_home(void);
esp_err_t lcd_disable_shift(void);
esp_err_t lcd_enable_shift(void);
esp_err_t lcd_scroll_line(uint8_t row, const char *text, uint32_t delay_ms);

#endif
