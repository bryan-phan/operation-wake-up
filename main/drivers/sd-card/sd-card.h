#ifndef SD_CARD_H
#define SD_CARD_H

#include "esp_err.h"

// Configures the SPI bus and mounts the card at /sdcard.
// After this returns ESP_OK, use normal fopen/fread/fwrite on that path.
esp_err_t sdc_init(void);

#endif
