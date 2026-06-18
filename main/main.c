

#include "esp_err.h"

#include "app_wifi.h"     
#include "player.h"       

#define SONG_URL "https://pub-b88a0936b6534894a6fbae56fbc30119.r2.dev/New%20Edition%20-%20Can%20You%20Stand%20The%20Rain%20(Official%20Music%20Video).mp3"

void app_main(void)
{
    ESP_ERROR_CHECK(app_wifi_connect());
    player_play(SONG_URL);
}
