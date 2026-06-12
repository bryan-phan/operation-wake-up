// ============================================================================
//  operation-wake-up  -- app entry point
//
//  The work is split into layers (see drivers/):
//     net    : HTTPS byte source
//     mp3    : MP3 -> PCM decode
//     audio  : PCM -> I2S -> MAX98357A
//     player : wires net -> mp3 -> audio (producer/consumer)
//
//  main just connects Wi-Fi and hands a URL to the player.
// ============================================================================

#include "esp_err.h"

#include "app_wifi.h"     // your Wi-Fi station driver
#include "player.h"       // the streaming MP3 player

#define SONG_URL "https://pub-b88a0936b6534894a6fbae56fbc30119.r2.dev/New%20Edition%20-%20Can%20You%20Stand%20The%20Rain%20(Official%20Music%20Video).mp3"

void app_main(void)
{
    ESP_ERROR_CHECK(app_wifi_connect());
    player_play(SONG_URL);
}
