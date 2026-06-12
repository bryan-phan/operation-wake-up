// Orchestration layer: wires the source (net) -> decode (mp3) -> sink (audio).
//
// Producer/consumer: the fetch task streams raw MP3 bytes into a stream buffer;
// the decode task drains it, decodes, and writes PCM to the amp. Two tasks keep
// the (stack-heavy) decoder out of the TLS/HTTP call chain and decouple download
// speed from playback speed.
#include "player.h"

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "esp_log.h"

#include "audio.h"
#include "net.h"
#include "mp3.h"

static const char *TAG = "player";

#define STREAM_BUF_SIZE (32 * 1024)   // raw MP3 bytes buffered between the tasks
#define READ_CHUNK      4096          // bytes drained from the buffer per pass

static StreamBufferHandle_t s_stream;
static volatile bool        s_fetch_done;
static const char          *s_url;
static bool                 s_rate_set;
static int16_t              s_stereo[MP3_MAX_SAMPLES];   // mono->stereo scratch

// ---- Producer: HTTPS bytes -> stream buffer --------------------------------
static void on_net_chunk(const char *data, size_t len, void *ctx)
{
    (void)ctx;
    const char *p = data;
    size_t n = len;
    while (n > 0) {
        size_t sent = xStreamBufferSend(s_stream, p, n, portMAX_DELAY);
        p += sent;
        n -= sent;
    }
}

static void fetch_task(void *arg)
{
    (void)arg;
    net_https_get(s_url, on_net_chunk, NULL);   // blocks until the file is done
    s_fetch_done = true;                         // tell the decoder it's the end
    vTaskDelete(NULL);
}

// ---- Consumer: stream buffer -> mp3 decode -> audio ------------------------
static void on_pcm(const int16_t *pcm, int samples, int channels, int hz, void *ctx)
{
    (void)ctx;
    if (!s_rate_set) {                  // match the amp's clock to the file
        audio_set_rate(hz);
        s_rate_set = true;
        ESP_LOGI(TAG, "decoding %d Hz, %d ch", hz, channels);
    }
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
        if (got > 0) {
            mp3_feed(buf, (int)got, on_pcm, NULL);
        }
        if (s_fetch_done && got == 0 && xStreamBufferIsEmpty(s_stream)) {
            break;
        }
    }
    ESP_LOGI(TAG, "playback finished");
    vTaskDelete(NULL);
}

// ---- Public ----------------------------------------------------------------
void player_play(const char *url)
{
    audio_init();

    s_stream = xStreamBufferCreate(STREAM_BUF_SIZE, 1);
    if (s_stream == NULL) {
        ESP_LOGE(TAG, "stream buffer alloc failed");
        return;
    }
    s_url        = url;
    s_fetch_done = false;

    // Consumer first (ready to drain), then the producer.
    xTaskCreate(decode_task, "decode", 24576, NULL, 5, NULL);   // big stack: minimp3
    xTaskCreate(fetch_task,  "fetch",   8192, NULL, 5, NULL);   // TLS + HTTP only
}
