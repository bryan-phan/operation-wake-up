#ifndef I2C_BUS_H
#define I2C_BUS_H

#include "driver/i2c_master.h"
#include "esp_err.h"

esp_err_t i2c_bus_init(void);
esp_err_t i2c_bus_add_device(const i2c_device_config_t *dev_config, i2c_master_dev_handle_t *dev_handle);
i2c_master_bus_handle_t i2c_bus_get_handle(void);

#endif
