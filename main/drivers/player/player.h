#ifndef PLAYER_H
#define PLAYER_H

// Stream an MP3 from `url` over HTTPS and play it on the amplifier.
// Brings up the audio output, then runs the fetch + decode tasks and returns.
// `url` must remain valid for the duration of playback (a string literal is fine).
void player_play(const char *url);

#endif
