// PS4 audio output thread (libSceAudioOut). The mixing itself is in mixer.cpp.
#include "audio.h"

#include <stdio.h>
#include <pthread.h>
#include <orbis/AudioOut.h>
#include <orbis/UserService.h>

static const int GRAIN = 256; // frames per sceAudioOutOutput call

static int32_t s_port = -1;
static pthread_t s_thread;
static bool s_started = false;
static int s_run = 0;

static void* audio_thread(void*) {
    static int16_t buf[2][GRAIN * 2];
    int cur = 0;
    while (__atomic_load_n(&s_run, __ATOMIC_ACQUIRE)) {
        audio_mix(buf[cur], GRAIN);
        sceAudioOutOutput(s_port, buf[cur]); // blocks until the hardware can take the next grain
        cur ^= 1;
    }
    sceAudioOutOutput(s_port, NULL); // wait for the last grain
    return NULL;
}

bool audio_init() {
    if (!audio_mixer_init()) printf("[nextbot] could not allocate sound effects\n");
    int rc = sceAudioOutInit();
    if (rc != 0) printf("[nextbot] sceAudioOutInit: %x\n", rc); // may report "already initialized"
    s_port = sceAudioOutOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
                             GRAIN, AUDIO_RATE, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (s_port <= 0) {
        printf("[nextbot] sceAudioOutOpen failed: %x\n", s_port);
        s_port = -1;
        return false;
    }
    __atomic_store_n(&s_run, 1, __ATOMIC_RELEASE);
    if (pthread_create(&s_thread, NULL, audio_thread, NULL) != 0) {
        printf("[nextbot] audio thread creation failed\n");
        sceAudioOutClose(s_port);
        s_port = -1;
        return false;
    }
    s_started = true;
    audio_mixer_set_thread_running(true);
    return true;
}

void audio_shutdown() {
    if (s_started) {
        audio_mixer_set_thread_running(false);
        __atomic_store_n(&s_run, 0, __ATOMIC_RELEASE);
        pthread_join(s_thread, NULL);
        s_started = false;
    }
    if (s_port >= 0) sceAudioOutClose(s_port);
    s_port = -1;
    audio_mixer_shutdown();
}
