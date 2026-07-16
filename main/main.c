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


#define URL "https://pub-b88a0936b6534894a6fbae56fbc30119.r2.dev/New%20Edition%20-%20Can%20You%20Stand%20The%20Rain%20(Official%20Music%20Video).mp3"
static const char *TAG = "mp3-download";

static void write_chunk(const char *data, size_t len, void *ctx)
{
    FILE *f = (FILE *)ctx;
    fwrite(data, 1, len, f);
}

void app_main(void)
{
    struct stat st;
    
    esp_err_t ret;
    ESP_ERROR_CHECK(sdc_init());

    if (stat("/sdcard/Rain.mp3", &st) != 0) {
        ESP_ERROR_CHECK(app_wifi_connect());
        FILE *f = fopen("/sdcard/Rain.mp3", "wb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            return;
        }

        ret = net_https_get(URL, write_chunk, f);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to download file");
            return;
        }
        fclose(f);
    }
    
    ESP_LOGI(TAG, "Playing the downloaded file");
    player_play("/sdcard/Rain.mp3");

}
