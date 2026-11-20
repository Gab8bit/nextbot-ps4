// Audio: one looping voice (the bot sound, with distance-based volume and pan) plus one
// voice for short synthesized UI effects. Mixing runs on a dedicated thread that feeds
// libSceAudioOut (48 kHz, 16-bit stereo, 256-frame grains).
#pragma once
#include <stdint.h>
#include "wav.h"

enum Sfx { SFX_MOVE, SFX_CONFIRM, SFX_BACK, SFX_CAUGHT, SFX_RECORD, SFX_COUNT };

bool audio_init();      // generates the effects and starts the output thread
void audio_shutdown();  // stops the thread and frees everything

// Starts looping s (nullptr = stop). Returns only when the audio thread no longer
// reads the previous sound, so the caller may free it right after.
void audio_set_loop(const Sound* s);
void audio_set_loop_gain(float volume, float pan); // volume 0..1, pan -1 (left) .. 1 (right)
void audio_set_master(float volume);               // 0..1
float audio_get_master();
void audio_play_sfx(Sfx id);

// Mixes 'frames' stereo frames into out (used by the thread; also by the PC test harness).
void audio_mix(int16_t* out, int frames);

// internal: shared by the mixer and the platform thread
bool audio_mixer_init();
void audio_mixer_shutdown();
void audio_mixer_set_thread_running(bool running);
