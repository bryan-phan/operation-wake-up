// Orchestration layer: wires the source (SD file) -> decode (mp3) -> sink (audio).
//
// Producer/consumer: the read task streams raw MP3 bytes into a stream buffer;
// the decode task drains it, decodes, and writes PCM to the amp. Two tasks keep
// the (stack-heavy) decoder off the caller's stack and decouple read speed from
// playback speed.
#include "player.h"

#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "esp_log.h"

#include "audio.h"
#include "mp3.h"

static const char *TAG = "player";

#define STREAM_BUF_SIZE (32 * 1024)   // raw MP3 bytes buffered between the tasks
#define READ_CHUNK      4096          // bytes drained from the buffer per pass
#define FILE_CHUNK      1024          // bytes read from the card per pass

static StreamBufferHandle_t s_stream;
static volatile bool        s_read_done;
static const char          *s_path;
static bool                 s_rate_set;
static int16_t              s_stereo[MP3_MAX_SAMPLES];   // mono->stereo scratch
static uint32_t             s_total_samples;
uint32_t decoded, skipped;

// ---- Producer: SD file -> stream buffer ------------------------------------
static void read_task(void *arg)
{
    (void)arg;

    FILE *f = fopen(s_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "failed to open %s", s_path);
        s_read_done = true;              // let the decoder finish and exit
        vTaskDelete(NULL);
        return;
    }

    uint8_t buf[FILE_CHUNK];
    size_t got;
    while ((got = fread(buf, 1, sizeof(buf), f)) > 0) {
        const uint8_t *p = buf;
        size_t n = got;
        while (n > 0) {                  // buffer may be full; send what fits, repeat
            size_t sent = xStreamBufferSend(s_stream, p, n, portMAX_DELAY);
            p += sent;
            n -= sent;
        }
    }

    fclose(f);
    s_read_done = true;                  // tell the decoder it's the end
    vTaskDelete(NULL);
}

// ---- Consumer: stream buffer -> mp3 decode -> audio ------------------------
static void on_pcm(const int16_t *pcm, int samples, int channels, int hz, void *ctx)
{
    (void)ctx;
    if (!s_rate_set) {                  // match the amp's clock to the file
        audio_set_rate(hz * .80);
        s_rate_set = true;
        ESP_LOGI(TAG, "decoding %d Hz, %d ch", hz, channels);
    }
    s_total_samples += samples;
    if (channels == 2) {
        audio_write(pcm, (size_t)samples * 2 * sizeof(int16_t));
    } else {                            // mono -> duplicate into both channels
        for (int i = 0; i < samples; i++) {
            s_stereo[2 * i]     = pcm[i];
            s_stereo[2 * i + 1] = pcm[i];
        }
        audio_write(s_stereo, (size_t)samples * 2 * sizeof(int16_t));
    }
}

static void decode_task(void *arg)
{
    (void)arg;
    static uint8_t buf[READ_CHUNK];
    mp3_reset();
    s_rate_set = false;

    for (;;) {
        size_t got = xStreamBufferReceive(s_stream, buf, sizeof(buf),
                                          pdMS_TO_TICKS(200));

        size_t avail = xStreamBufferBytesAvailable(s_stream);
        ESP_LOGI(TAG, "buf: %u / %u", (unsigned)avail, STREAM_BUF_SIZE);

        if (got > 0) {
            mp3_feed(buf, (int)got, on_pcm, NULL);
        }
        if (s_read_done && got == 0 && xStreamBufferIsEmpty(s_stream)) {
            break;
        }
    }
    ESP_LOGI(TAG, "playback finished");
    ESP_LOGI(TAG, "total samples: %u", (unsigned)s_total_samples);
    vTaskDelete(NULL);
}

// ---- Public ----------------------------------------------------------------
void player_play(const char *path)
{
    audio_init();
    s_total_samples = 0;
    mp3_stats(&decoded, &skipped);
    ESP_LOGI(TAG, "frames: %u decoded, %u skipped, %u total",
         (unsigned)decoded, (unsigned)skipped, (unsigned)(decoded + skipped));

    s_stream = xStreamBufferCreate(STREAM_BUF_SIZE, 1);
    if (s_stream == NULL) {
        ESP_LOGE(TAG, "stream buffer alloc failed");
        return;
    }
    s_path      = path;
    s_read_done = false;

    // Consumer first (ready to drain), then the producer.
    xTaskCreate(decode_task, "decode", 24576, NULL, 5, NULL);   // big stack: minimp3
    xTaskCreate(read_task,   "read",    4096, NULL, 5, NULL);   // fread only
}
