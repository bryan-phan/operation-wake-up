#include "driver/i2c_master.h"
#include "esp_check.h"
#include "i2c_bus.h"

#define I2C_BUS_PORT        I2C_NUM_0
#define I2C_BUS_SDA         21
#define I2C_BUS_SCL         22
#define I2C_BUS_GLITCH_CNT  7

static i2c_master_bus_handle_t s_bus_handle;

// Master bus init
esp_err_t i2c_bus_init(void) {
    if (s_bus_handle != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_config_t config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_BUS_PORT,
        .scl_io_num = I2C_BUS_SCL,
        .sda_io_num = I2C_BUS_SDA,
        .glitch_ignore_cnt = I2C_BUS_GLITCH_CNT,
        .flags.enable_internal_pullup = true,
    };

    return i2c_new_master_bus(&config, &s_bus_handle);
}

// Attach slave device
esp_err_t i2c_bus_add_device(const i2c_device_config_t *dev_config, i2c_master_dev_handle_t *dev_handle) {
    ESP_RETURN_ON_ERROR(i2c_bus_init(), "i2c_bus", "failed to initialize I2C bus");
    return i2c_master_bus_add_device(s_bus_handle, dev_config, dev_handle);
}

// Retrieve master bus handle
i2c_master_bus_handle_t i2c_bus_get_handle(void) {
    return s_bus_handle;
}
