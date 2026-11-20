// Minimal RIFF/WAVE loader. Output is always 48 kHz, 16-bit, stereo interleaved.
// Accepts PCM 8/16/24/32-bit and float 32-bit, mono/stereo/multichannel (first two
// channels are kept), any sample rate from 8 to 192 kHz (linear resampling).
#pragma once
#include <stddef.h>
#include <stdint.h>

constexpr int AUDIO_RATE = 48000;

struct Sound {
    int16_t* data = nullptr; // frames * 2 samples (L, R)
    uint32_t frames = 0;
};

bool wav_parse(const unsigned char* buf, size_t len, Sound* out, char* err, int errLen, uint32_t maxSeconds);
bool wav_load(const char* path, Sound* out, char* err, int errLen, uint32_t maxSeconds);
void sound_free(Sound* s);
