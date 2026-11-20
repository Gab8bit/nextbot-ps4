// PC (Linux) stand-ins for the PS4-only platform functions, used by the test harness.
// Not part of the PS4 build.
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "platform/audio.h"
#include "platform/input.h"
#include "platform/system.h"

uint32_t g_hostLight = 0;
uint8_t g_hostRumble = 0;

bool input_init(int) { return true; }
void input_read(PadRaw* out) { memset(out, 0, sizeof(*out)); out->connected = true; }
void input_set_feedback(uint32_t light, uint8_t big, uint8_t) { g_hostLight = light; g_hostRumble = big; }
void input_shutdown() {}

uint64_t system_time_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + ts.tv_nsec / 1000;
}
void system_sleep_us(uint32_t us) { usleep(us); }
void system_hide_splash() {}

// No output thread on the PC: the harness calls audio_mix() itself.
bool audio_init() { return audio_mixer_init(); }
void audio_shutdown() { audio_mixer_shutdown(); }
