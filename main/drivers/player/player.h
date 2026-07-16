#ifndef PLAYER_H
#define PLAYER_H

// Play an MP3 from `path` on the SD card (e.g. "/sdcard/song.mp3") on the amplifier.
// Brings up the audio output, then runs the read + decode tasks and returns.
// The card must already be mounted. `path` must remain valid for the duration of
// playback (a string literal is fine).
void player_play(const char *path);

#endif