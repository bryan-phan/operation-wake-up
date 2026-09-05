#ifndef MP3_H
#define MP3_H

#include <stdint.h>

// Largest interleaved PCM frame a decode can produce (1152 samples * 2 channels).
#define MP3_MAX_SAMPLES (1152 * 2)

// Called for each decoded frame.
//   pcm      : interleaved 16-bit samples (samples * channels values)
//   samples  : samples per channel
//   channels : 1 (mono) or 2 (stereo)
//   hz       : sample rate of this frame
typedef void (*mp3_pcm_cb_t)(const int16_t *pcm, int samples, int channels,
                             int hz, void *ctx);

// Reset the decoder before starting a new stream.
void mp3_reset(void);

// Feed raw MP3 bytes. Complete frames are decoded and handed to `cb`; partial
// frames are buffered internally until the rest arrives. A leading ID3 tag is
// skipped automatically.
void mp3_feed(const uint8_t *data, int len, mp3_pcm_cb_t cb, void *ctx);

// Decode whatever bytes remain buffered. Call once, at end of stream: `mp3_feed`
// deliberately holds back a partial frame's worth of data, and this releases it.
void mp3_flush(mp3_pcm_cb_t cb, void *ctx);

void mp3_stats(uint32_t *decoded, uint32_t *skipped);

#endif
