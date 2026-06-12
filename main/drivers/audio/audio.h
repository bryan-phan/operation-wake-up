#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>
#include <stdint.h>

// Set up the I2S peripheral that drives the MAX98357A amplifier
// (TX-only, master, standard I2S, no MCLK).
void audio_init(void);

// Stream a block of decoded 16-bit PCM samples to the amplifier.
// Blocks until the data has been queued to the I2S DMA.
void audio_write(const int16_t *pcm, size_t bytes);

// Re-point the I2S clock at a new sample rate (e.g. to match a decoded file).
void audio_set_rate(int hz);

#endif
