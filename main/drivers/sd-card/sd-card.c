#include "sd-card.h"

#include <stdio.h>

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"

static const char *TAG = "MYSDCARD";

#define PIN_NUM_MISO  19
#define PIN_NUM_MOSI  23
#define PIN_NUM_CLK   18
#define PIN_NUM_CS    2

#define MOUNT_POINT "/sdcard"

// Configure SPI bus
static esp_err_t spi_bus_config(sdmmc_host_t *host){

    esp_err_t ret;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host->slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ret;
    }
    else{
        ESP_LOGI(TAG, "SPI INTIALIZED.");
        return ret;
    }


}

// Slots to SPI Bus and Mounts SD Card
static esp_err_t sdc_setup(sdmmc_host_t *host, sdmmc_card_t **card){
    esp_err_t ret;

     ESP_LOGI(TAG, "Initializing SD card");
    // Slot to SPI Bus
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host->slot;
    ESP_LOGI(TAG, "Using SPI peripheral");

    // Mount SD Card
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    ESP_LOGI(TAG, "Mounting filesystem");


    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, host, &slot_config, &mount_config, card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem.");
            return ret;
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card.");
            return ret;
            }
        }
    else{
        ESP_LOGI(TAG, "Filesystem mounted");
        return ret;
    }
}

esp_err_t sdc_init(void)
{
    esp_err_t ret;

    // Configure host
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 5000;

    ret = spi_bus_config(&host);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure SPI bus.");
        return ret;
    }

    // Configure SD card and mount
    sdmmc_card_t *card;
    ret = sdc_setup(&host, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup SD card.");
        return ret;
    }
    else{
        ESP_LOGI(TAG, "SD card setup complete.");
    }
    sdmmc_card_print_info(stdout, card);

    return ESP_OK;
}
