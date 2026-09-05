# ESP32 Networked MP3 Player

A streaming MP3 player for the ESP32. It connects to Wi-Fi, downloads a track
over HTTPS to a microSD card, and plays it through an I2S amplifier — decoding
on the fly, without loading the whole file into memory.

Written in C on ESP-IDF as a set of independent driver modules.

## Signal flow

```
HTTPS GET ──> SD card ──> read task ──> 32 KB stream buffer ──> decode task ──> I2S ──> MAX98357A
 (net.c)     (sd-card.c)  (player.c)                           (mp3.c/minimp3)  (audio.c)   amp
```

A producer/consumer pair of FreeRTOS tasks moves data across a 32 KB stream
buffer, so SD reads run ahead of playback and the decoder never starves.

## What works

- **Networked download** — Wi-Fi station mode with retry, then a streaming
  HTTPS GET whose server certificate is checked against the bundled Mozilla CA
  roots. The response body is written to SD chunk by chunk (`net.c`, `app_wifi.c`).
- **SD storage** — microSD over SPI, mounted as FATFS at `/sdcard`; playback
  reads it with plain `fopen`/`fread` (`sd-card.c`).
- **Streaming MP3 decode** — a wrapper over minimp3 that owns a 16 KB sliding
  input window, skips a leading ID3v2 tag, resyncs past corrupt frames, and
  holds back partial frames so the decoder's bit reservoir isn't clobbered
  (`mp3.c`).
- **I2S audio out** — TX-only I2S driving a MAX98357A class-D amp, 16-bit PCM,
  with the clock reconfigured at runtime to match the decoded file's sample
  rate and mono upmixed to stereo (`audio.c`, `player.c`).
- **Shared I2C peripherals** — a single I2C master bus shared by a DS1307 RTC
  and a 16×2 HD44780 LCD (`i2c_bus.c`, `rtc.c`, `lcd.c`).

## Hardware

| Part | Interface | Pins |
| --- | --- | --- |
| MAX98357A amplifier | I2S | BCLK 26, LRCLK 25, DOUT 27 |
| microSD card | SPI | MISO 19, MOSI 23, CLK 18, CS 2 |
| DS1307 RTC (`0x68`) | I2C | SDA 21, SCL 22 |
| HD44780 LCD via PCF8574 (`0x27`) | I2C | SDA 21, SCL 22 (shared) |
| Buttons ×3 (active-low) | GPIO | 17, 16, 32 |

## Build

Built with ESP-IDF (v6.0.1), target `esp32`.

```bash
# provide Wi-Fi credentials (gitignored)
cp main/drivers/wifi/include/wifi_credentials.h.example \
   main/drivers/wifi/include/wifi_credentials.h
# edit WIFI_SSID / WIFI_PASS

idf.py build flash monitor
```

The `espressif/button` component is pulled automatically by the IDF Component
Manager (`idf_component.yml`).

## Status

The player pipeline above is complete and runs end to end. This branch also
carries the start of an **alarm clock** built on the same hardware — a DS1307
time source, an LCD readout, and three buttons cycling run / set-clock /
set-alarm modes. The alarm scheduling and set-time UI are still in progress;
the audio player is the finished core it's built on.
