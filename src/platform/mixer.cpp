// Portable mixer. The game thread only publishes requests through atomics; the audio
// thread owns all playback state, so no locks are needed.
// (libc++'s <atomic> does not compile with OpenOrbis, hence the __atomic builtins.)
#include "audio.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "system.h"

static Sound s_sfx[SFX_COUNT];

// requests (written by the game thread)
static const Sound* s_reqLoop = nullptr;
static int s_reqSeq = 0, s_ackSeq = 0;
static const Sound* s_reqSfx = nullptr;
static int s_sfxSeq = 0;
static int s_volume = 0;        // 0..65536
static int s_pan = 0;           // -65536..65536
static int s_master = 52429;    // 0..65536 (80 %)
static bool s_threadRunning = false;

// playback state (audio thread only)
static const Sound* m_loop = nullptr;
static uint32_t m_loopPos = 0;
static int m_lastSeq = 0;
static const Sound* m_sfx = nullptr;
static uint32_t m_sfxPos = 0;
static int m_lastSfxSeq = 0;
static float m_gainL = 0, m_gainR = 0;

static inline int ld(int* p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }
static inline void st(int* p, int v) { __atomic_store_n(p, v, __ATOMIC_RELEASE); }

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

// ---- synthesized effects ----------------------------------------------------
static bool synth_alloc(Sound* s, float seconds) {
    s->frames = (uint32_t)(seconds * AUDIO_RATE);
    s->data = (int16_t*)calloc(s->frames, 4);
    if (!s->data) { s->frames = 0; return false; }
    return true;
}

// Adds a tone segment: square-ish wave (soft) with a linear frequency sweep and decay.
static void synth_tone(Sound* s, float start, float dur, float f0, float f1, float amp) {
    uint32_t a = (uint32_t)(start * AUDIO_RATE), n = (uint32_t)(dur * AUDIO_RATE);
    float phase = 0;
    for (uint32_t i = 0; i < n && a + i < s->frames; i++) {
        float t = (float)i / n;
        float f = f0 + (f1 - f0) * t;
        phase += f / AUDIO_RATE;
        phase -= (int)phase;
        float v = sinf(phase * 6.2831853f);
        v = clampf(v * 2.5f, -1, 1);                     // soft square
        float env = (1.0f - t) * (i < 200 ? i / 200.0f : 1.0f);
        int sample = (int)(v * env * amp * 32767.0f);
        int idx = (a + i) * 2;
        int l = s->data[idx] + sample;
        s->data[idx] = s->data[idx + 1] = (int16_t)(l > 32767 ? 32767 : (l < -32768 ? -32768 : l));
    }
}

static void synth_noise(Sound* s, float dur, float amp) {
    uint32_t n = (uint32_t)(dur * AUDIO_RATE), seed = 12345;
    float lp = 0;
    for (uint32_t i = 0; i < n && i < s->frames; i++) {
        seed = seed * 1103515245u + 12345u;
        float r = ((seed >> 9) & 0xFFFF) / 32768.0f - 1.0f;
        lp += (r - lp) * 0.25f;                          // darker noise
        float env = 1.0f - (float)i / n;
        env *= env;
        int v = s->data[i * 2] + (int)(lp * env * amp * 32767.0f);
        s->data[i * 2] = s->data[i * 2 + 1] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
    }
}

bool audio_mixer_init() {
    bool ok = true;
    ok &= synth_alloc(&s_sfx[SFX_MOVE], 0.05f);
    if (s_sfx[SFX_MOVE].data) synth_tone(&s_sfx[SFX_MOVE], 0, 0.05f, 700, 760, 0.25f);
    ok &= synth_alloc(&s_sfx[SFX_CONFIRM], 0.16f);
    if (s_sfx[SFX_CONFIRM].data) {
        synth_tone(&s_sfx[SFX_CONFIRM], 0, 0.07f, 880, 880, 0.3f);
        synth_tone(&s_sfx[SFX_CONFIRM], 0.07f, 0.09f, 1320, 1320, 0.3f);
    }
    ok &= synth_alloc(&s_sfx[SFX_BACK], 0.12f);
    if (s_sfx[SFX_BACK].data) synth_tone(&s_sfx[SFX_BACK], 0, 0.12f, 520, 300, 0.3f);
    ok &= synth_alloc(&s_sfx[SFX_CAUGHT], 1.2f);
    if (s_sfx[SFX_CAUGHT].data) {
        synth_noise(&s_sfx[SFX_CAUGHT], 0.9f, 0.7f);
        synth_tone(&s_sfx[SFX_CAUGHT], 0, 1.2f, 110, 40, 0.6f);
    }
    ok &= synth_alloc(&s_sfx[SFX_RECORD], 0.6f);
    if (s_sfx[SFX_RECORD].data) {
        const float notes[4] = { 523.25f, 659.25f, 783.99f, 1046.5f };
        for (int i = 0; i < 4; i++) synth_tone(&s_sfx[SFX_RECORD], i * 0.1f, i == 3 ? 0.3f : 0.12f, notes[i], notes[i], 0.3f);
    }
    return ok;
}

void audio_mixer_shutdown() {
    for (int i = 0; i < SFX_COUNT; i++) sound_free(&s_sfx[i]);
    m_loop = nullptr;
    m_sfx = nullptr;
}

void audio_mixer_set_thread_running(bool running) { __atomic_store_n(&s_threadRunning, running, __ATOMIC_RELEASE); }

// ---- game-thread API ----------------------------------------------------------
void audio_set_loop(const Sound* s) {
    __atomic_store_n(&s_reqLoop, s, __ATOMIC_RELEASE);
    int seq = ld(&s_reqSeq) + 1;
    st(&s_reqSeq, seq);
    if (!__atomic_load_n(&s_threadRunning, __ATOMIC_ACQUIRE)) return;
    // wait (max ~0.5 s) until the audio thread has switched to the new sound
    for (int i = 0; i < 500 && ld(&s_ackSeq) != seq; i++) system_sleep_us(1000);
}

void audio_set_loop_gain(float volume, float pan) {
    st(&s_volume, (int)(clampf(volume, 0, 1) * 65536.0f));
    st(&s_pan, (int)(clampf(pan, -1, 1) * 65536.0f));
}

void audio_set_master(float v) { st(&s_master, (int)(clampf(v, 0, 1) * 65536.0f)); }
float audio_get_master() { return ld(&s_master) / 65536.0f; }

void audio_play_sfx(Sfx id) {
    if (id < 0 || id >= SFX_COUNT || !s_sfx[id].data) return;
    __atomic_store_n(&s_reqSfx, &s_sfx[id], __ATOMIC_RELEASE);
    st(&s_sfxSeq, ld(&s_sfxSeq) + 1);
}

// ---- audio-thread mixing --------------------------------------------------------
void audio_mix(int16_t* out, int frames) {
    int rs = ld(&s_reqSeq);
    if (rs != m_lastSeq) {
        m_loop = __atomic_load_n(&s_reqLoop, __ATOMIC_ACQUIRE);
        m_loopPos = 0;
        m_lastSeq = rs;
        st(&s_ackSeq, rs);
    }
    int ss = ld(&s_sfxSeq);
    if (ss != m_lastSfxSeq) {
        m_sfx = __atomic_load_n(&s_reqSfx, __ATOMIC_ACQUIRE);
        m_sfxPos = 0;
        m_lastSfxSeq = ss;
    }

    float master = ld(&s_master) / 65536.0f;
    float vol = ld(&s_volume) / 65536.0f * master;
    float pan = ld(&s_pan) / 65536.0f;
    float tL = vol * clampf(1.0f - pan, 0, 1), tR = vol * clampf(1.0f + pan, 0, 1);
    // ramp the gain across the block to avoid clicks when the volume changes
    float dL = (tL - m_gainL) / frames, dR = (tR - m_gainR) / frames;

    for (int i = 0; i < frames; i++) {
        float l = 0, r = 0;
        m_gainL += dL; m_gainR += dR;
        if (m_loop && m_loop->frames) {
            const int16_t* p = m_loop->data + (size_t)m_loopPos * 2;
            l = p[0] * m_gainL;
            r = p[1] * m_gainR;
            if (++m_loopPos >= m_loop->frames) m_loopPos = 0;
        }
        if (m_sfx) {
            const int16_t* p = m_sfx->data + (size_t)m_sfxPos * 2;
            l += p[0] * master;
            r += p[1] * master;
            if (++m_sfxPos >= m_sfx->frames) m_sfx = nullptr;
        }
        out[i * 2]     = (int16_t)clampf(l, -32768.0f, 32767.0f);
        out[i * 2 + 1] = (int16_t)clampf(r, -32768.0f, 32767.0f);
    }
    m_gainL = tL; m_gainR = tR;
}
