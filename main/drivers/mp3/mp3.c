// Streaming MP3 decoder: raw MP3 bytes in -> PCM frames out.
// Wraps minimp3 and owns the sliding input window, frame framing, and ID3 skip.
#include "mp3.h"
#include "esp_log.h"

#include <string.h>
#include <stdbool.h>

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"               // single-header decoder, lives in main/

#define WINDOW_SIZE (16 * 1024)    // sliding input window
#define DECODE_FLOOR 2560          // > minimp3's max frame (2304) + next header

static mp3dec_t s_dec;
static uint8_t  s_win[WINDOW_SIZE];
static int      s_win_len;
static bool     s_id3_checked;
static uint32_t s_id3_skip;
static int16_t  s_pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
static uint32_t s_decoded, s_skipped;

void mp3_reset(void)
{
    mp3dec_init(&s_dec);
    s_win_len     = 0;
    s_id3_checked = false;
    s_id3_skip    = 0;
    s_decoded     = 0;
    s_skipped     = 0;
}

// Decode every complete frame currently in the window, then keep the leftover.
// `flush` drains the tail at end of stream, where no more bytes are coming.
static void decode_window(mp3_pcm_cb_t cb, void *ctx, bool flush)
{
    int pos = 0;
    for (;;) {
        int avail = s_win_len - pos;
        if (avail <= 0) {
            break;
        }
        // minimp3 can't tell a truncated frame from garbage: if the frame it
        // finds doesn't fit in the buffer we pass, it reports the whole
        // remainder as consumed, returns no samples, and memsets its own state
        // -- eating the partial frame and the bit reservoir with it, which then
        // costs the next frame or two as well. Never hand it less than one
        // max-size frame unless the stream has ended.
        if (avail < DECODE_FLOOR && !flush) {
            break;
        }

        mp3dec_frame_info_t info;
        int samples = mp3dec_decode_frame(&s_dec, s_win + pos, avail, s_pcm, &info);
        if (info.frame_bytes == 0) {
            break;                          // need more bytes
        }
        pos += info.frame_bytes;
        if (samples > 0 && cb) {
            cb(s_pcm, samples, info.channels, info.hz, ctx);
            s_decoded++;
        }
        else{
            s_skipped++;
            if (s_skipped < 20) {
                ESP_LOGW("mp3", "skip #%u: frame_bytes=%d hz=%d ch=%d layer=%d br=%d win_len=%d pos=%d",
                 (unsigned)s_skipped, info.frame_bytes, info.hz, info.channels,
                 info.layer, info.bitrate_kbps, s_win_len, pos);
            }
        }
    }


    if (pos > 0) {
        memmove(s_win, s_win + pos, (size_t)(s_win_len - pos));
        s_win_len -= pos;
    } else if (s_win_len == WINDOW_SIZE) {
        // Full window, no frame found (corrupt data): skip a byte to resync.
        memmove(s_win, s_win + 1, (size_t)(s_win_len - 1));
        s_win_len -= 1;
    }
}

void mp3_feed(const uint8_t *data, int len, mp3_pcm_cb_t cb, void *ctx)
{
    const uint8_t *p = data;
    int n = len;

    // Skip a leading ID3v2 tag (assumes its 10-byte header is in the first feed).
    if (!s_id3_checked) {
        if (n >= 10 && p[0] == 'I' && p[1] == 'D' && p[2] == '3') {
            s_id3_skip = 10 + (((uint32_t)(p[6] & 0x7f) << 21) |
                               ((uint32_t)(p[7] & 0x7f) << 14) |
                               ((uint32_t)(p[8] & 0x7f) <<  7) |
                               ((uint32_t)(p[9] & 0x7f)));
        }
        s_id3_checked = true;
    }
    if (s_id3_skip) {
        uint32_t d = (s_id3_skip < (uint32_t)n) ? s_id3_skip : (uint32_t)n;
        s_id3_skip -= d; p += d; n -= (int)d;
        if (n <= 0) return;
    }

    // Append into the window and decode, looping if the input is larger than
    // the free space.
    while (n > 0) {
        int space = WINDOW_SIZE - s_win_len;
        if (space == 0) {
            s_win_len = 0;                  // safety: avoid a deadlock
            space = WINDOW_SIZE;
        }
        int take = (n < space) ? n : space;
        memcpy(s_win + s_win_len, p, (size_t)take);
        s_win_len += take;
        p += take;
        n -= take;
        decode_window(cb, ctx, false);
    }
}

void mp3_flush(mp3_pcm_cb_t cb, void *ctx)
{
    decode_window(cb, ctx, true);   // decode what's left below DECODE_FLOOR
}

void mp3_stats(uint32_t *decoded, uint32_t *skipped)
{
    if (decoded) *decoded = s_decoded;
    if (skipped) *skipped = s_skipped;
}