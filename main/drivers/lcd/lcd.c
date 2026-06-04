#include <stdio.h>
#include <string.h>
#include "driver/i2c_master.h"
#include "rom/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.h"

#define LCD_SDA         21
#define LCD_SCL         22
#define LCD_ADDR        0x27
#define LCD_SPEED_HZ    100000

#define LCD_ENABLE      0x04
#define LCD_RS          0x01
#define LCD_BACKLIGHT   0x08
#define LCD_COLS        16
#define LCD_ROWS        2

/*
NEVER PRE-OPTIMIZE. YOU CAN SET DEFINES AND SHIT LATER. JUST MAKE EVERYTHING WORK.
That way, you can look back at it and review and you'll actually grasp what you did.
*/

//-----------------------------------------
// I2C INIT
//-----------------------------------------

static i2c_master_bus_handle_t bus_handle; // A mere reference to the ACTUAL bus being used.
static i2c_master_dev_handle_t lcd_handle;

// Intialize i2c pin and devices
void i2c_init(void) {

    // This sets up the hardware
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0, // Hardware Block (0 or 1)
        .scl_io_num = LCD_SCL, // SCL
        .sda_io_num = LCD_SDA, // SDA
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    // This configures the hardware block and binds it to SCL/SDA
    // After the bus is created, you can add devices to the bus.
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle)); // Now you have the bus with the configurations you set up from before.

    // ATP, you start adding the devices you want to add. So best start looking at the datasheet and fill in the blanks.
    // In this case, I'm configuring the LCD device so rename appropriately
    i2c_device_config_t lcd_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = LCD_ADDR,
        .scl_speed_hz = LCD_SPEED_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &lcd_cfg, &lcd_handle));

}

//-----------------------------------
// HLF8574 Write
//-----------------------------------

/*
The LCD I have is apparently off-brand.
The backpack it uses is the HLF8574 backpack.
** We use uint8_t since it can only move 8-bits at a time.
*/

// Read and send a byte (data) to the HLF8574 over I2C for the HD44780 to read (in simple terms ig)
esp_err_t HLF8574_write(uint8_t data) {
    return i2c_master_transmit(lcd_handle, &data, 1, -1);
}

//-----------------------------------
// HD447780U Behaviors
//-----------------------------------

/*
The entire purpose of pulse is to just pulse the enable pin (bit 2) on "data" once [LHL].
By doing this, you're sending a nibble (half a byte) to the LCD to read.
So use bitwise operations to achieve that.
Note that you're still using "data" but you're just flipping one bit.
*/

// To send the data (nibble) over to the backpack
esp_err_t lcd_pulse_enable(uint8_t data) {
    // Low
    ESP_ERROR_CHECK(HLF8574_write(data & (uint8_t)~LCD_ENABLE)); // Make sure to typecast so that you can compare 8 bits lol
    ets_delay_us(5);

    // High
    ESP_ERROR_CHECK(HLF8574_write(data | (uint8_t)LCD_ENABLE));
    ets_delay_us(5);

    // Low
    ESP_ERROR_CHECK(HLF8574_write(data & (uint8_t)~LCD_ENABLE));
    ets_delay_us(5);

    return ESP_OK; // No point in ESP_ERROR_CHECK if it returns this anyway lol

}

// Writes the nibble to the data and pulse (send) it through
esp_err_t lcd_write_nibble(uint8_t nibble, uint8_t rs){
    uint8_t data = ((nibble & 0x0F) << 4) | LCD_BACKLIGHT | (rs ? LCD_RS : 0);
    return lcd_pulse_enable(data);
}

// Sends the full 8 bit over to the HD44780
// Sends two 4 bit nibbles to make the full byte.
esp_err_t lcd_send(uint8_t byte, uint8_t rs){
    esp_err_t ret;

    ret = lcd_write_nibble(byte >> 4, rs); // high nibble
    if (ret != ESP_OK) return ret;

    ret = lcd_write_nibble(byte & 0x0F, rs); // low nibble
    if (ret != ESP_OK) return ret;

    ets_delay_us(50);

    return ESP_OK;
}

//-------------------------------------------------
// At this point, we need to decide whether the byte is an instruction or data register

esp_err_t lcd_cmd(uint8_t cmd){
    esp_err_t ret = lcd_send(cmd, 0); // 0 for Instruction
    if (ret != ESP_OK) return ret;

    if (cmd == 0x01 || cmd == 0x02) ets_delay_us(2000);

    return ESP_OK;
}

esp_err_t lcd_char(char c){
    return lcd_send((uint8_t)c, 1); // 1 for Data
}

// Note: Because they get sent as nibbles, the RS/RW/E travel alongside as our constants above.
// In other words, by the time the backpack is sending it to the LCD, the upper half of the byte always carries the RS/RW/E signals.

//----------------------------------------------------------
// LCD INIT !!! (Don't forget that you haven't actually initialized the LCD display (device). Only the backpack.)

void lcd_init(void){

    // TaskDelay measures in ticks
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_ERROR_CHECK(HLF8574_write(LCD_BACKLIGHT));

    // 4-bit init sequence
    ESP_ERROR_CHECK(lcd_write_nibble(0x03, 0));
    ets_delay_us(4500);

    ESP_ERROR_CHECK(lcd_write_nibble(0x03, 0));
    ets_delay_us(150);

    ESP_ERROR_CHECK(lcd_write_nibble(0x03, 0));
    ESP_ERROR_CHECK(lcd_write_nibble(0x02, 0));

    // Now for the commands
    ESP_ERROR_CHECK(lcd_cmd(0x28)); // 4-bit, 2 lines, 5x8
    ESP_ERROR_CHECK(lcd_cmd(0x08)); // Display off
    ESP_ERROR_CHECK(lcd_cmd(0x0C)); // Display On
    ESP_ERROR_CHECK(lcd_cmd(0x01)); // Display clear
    ESP_ERROR_CHECK(lcd_cmd(0x06)); // Move cursor, no shift

}

//--------------------------------------------------------------------------------------------
// FUNCTIONALITY
//--------------------------------------------------------------------------------------------

// Moves the cursor
esp_err_t lcd_set_cursor(uint8_t col, uint8_t row){
    // For line 1 and 2, respectively
    uint8_t row_offsets[] = {0x00, 0x40};

    if (row >= LCD_ROWS) return ESP_ERR_INVALID_ARG;

    return lcd_cmd(0x80 | (col + row_offsets[row]));
}

esp_err_t lcd_print_line(uint8_t row, const char *str){
    esp_err_t ret;

    if (row >= LCD_ROWS) return ESP_ERR_INVALID_ARG;

    ret = lcd_set_cursor(0, row);
    if (ret != ESP_OK) return ret;

    for (uint8_t i = 0; i < LCD_COLS; i++){

        char c = (str[i] != '\0' ? str[i] : ' ');

        ret = lcd_char(c);
        if (ret != ESP_OK) return ret;

    }

    return ESP_OK;
}

esp_err_t lcd_print(const char *str){

    esp_err_t ret;
    while (*str){

        ret = lcd_char(*str);
        if (ret != ESP_OK) return ret;

        str++;
    }
    return ESP_OK;
}

esp_err_t lcd_clear(void){
    return lcd_cmd(0x01);
}

esp_err_t lcd_home(void){
    return lcd_cmd(0x02);
}

esp_err_t lcd_disable_shift(void){
    return lcd_cmd(0x06);
}

esp_err_t lcd_enable_shift(void){
    return lcd_cmd(0x07);
}

esp_err_t lcd_scroll_line(uint8_t row, const char *text, uint32_t delay_ms){
    char window[LCD_COLS + 1];
    size_t len = strlen(text);

    if (row >= LCD_ROWS) return ESP_ERR_INVALID_ARG;

    if (len <= LCD_COLS) return lcd_print_line(row, text);

    for (size_t i = 0; i <= len - LCD_COLS; i++){
        memcpy(window, text + i, LCD_COLS);
        window[LCD_COLS] = '\0';

        ESP_ERROR_CHECK(lcd_print_line(row, window));
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    return ESP_OK;
}
