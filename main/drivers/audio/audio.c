#include "audio.h"

#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "driver/i2s_std.h"  
#include "esp_log.h" 

// ----- MAX98357A I2S pins -----
#define I2S_BCLK    GPIO_NUM_26   // -> amp BCLK
#define I2S_LRCLK   GPIO_NUM_25   // -> amp LRC
#define I2S_DOUT    GPIO_NUM_27   // -> amp DIN

// Set to MP3's default sample rate. If your MP3s use a different rate, read it from the decoder and reconfigure the I2S driver.
// (the MAX98357A only accepts 8/16/32/44.1/48/88.2/96 kHz).
#define SAMPLE_RATE 44100

static i2s_chan_handle_t s_tx;
static const char *TAG = "audio";

void audio_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,   // MAX98357A requires no master clock
            .bclk = I2S_BCLK,
            .ws   = I2S_LRCLK,
            .dout = I2S_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
}

void audio_write(const int16_t *pcm, size_t bytes)
{
    size_t written = 0;
    esp_err_t err = i2s_channel_write(s_tx, pcm, bytes, &written, portMAX_DELAY);
    if (err != ESP_OK || written != bytes) {
        ESP_LOGW(TAG, "short write: %u of %u (%s)", (unsigned)written, (unsigned)bytes,
                 esp_err_to_name(err));
    }
}

// Re-point the I2S clock at a new sample rate (called once, to match the file).
void audio_set_rate(int hz)
{
    i2s_channel_disable(s_tx);
    i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(hz);
    ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(s_tx, &clk));
    i2s_channel_enable(s_tx);
}
