#include "wav.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"

static const size_t WAV_MAX_FILE = 128u * 1024u * 1024u;

static uint16_t rd16(const unsigned char* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const unsigned char* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

void sound_free(Sound* s) {
    if (s->data) free(s->data);
    s->data = nullptr;
    s->frames = 0;
}

// Reads one sample of the given format and returns it as int16.
static int16_t read_sample(const unsigned char* p, int bits, bool isFloat) {
    if (isFloat) {
        float f;
        memcpy(&f, p, 4);
        if (!(f == f)) f = 0;          // NaN
        if (f > 1.0f) f = 1.0f;
        if (f < -1.0f) f = -1.0f;
        return (int16_t)(f * 32767.0f);
    }
    switch (bits) {
        case 8:  return (int16_t)(((int)p[0] - 128) * 256);          // 8-bit WAV is unsigned
        case 16: return (int16_t)rd16(p);
        case 24: return (int16_t)(p[1] | (p[2] << 8));                 // keep the top 16 bits
        case 32: return (int16_t)(p[2] | (p[3] << 8));
    }
    return 0;
}

bool wav_parse(const unsigned char* buf, size_t len, Sound* out, char* err, int errLen, uint32_t maxSeconds) {
    out->data = nullptr; out->frames = 0;
    if (len < 12 || memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0) {
        snprintf(err, errLen, "non è un file WAV (RIFF/WAVE)");
        return false;
    }
    int format = -1, channels = 0, bits = 0, blockAlign = 0;
    uint32_t rate = 0;
    const unsigned char* data = nullptr;
    size_t dataLen = 0;

    // Walk the chunks; "fmt " and "data" may appear in any order, unknown chunks are skipped.
    size_t pos = 12;
    while (pos + 8 <= len) {
        const unsigned char* ck = buf + pos;
        uint32_t size = rd32(ck + 4);
        size_t body = pos + 8;
        size_t avail = len - body;
        if (memcmp(ck, "fmt ", 4) == 0 && size >= 16 && avail >= 16) {
            format = rd16(ck + 8);
            channels = rd16(ck + 10);
            rate = rd32(ck + 12);
            blockAlign = rd16(ck + 20);
            bits = rd16(ck + 22);
            if (format == 0xFFFE && size >= 40 && avail >= 40) format = rd16(ck + 32); // WAVE_FORMAT_EXTENSIBLE
        } else if (memcmp(ck, "data", 4) == 0) {
            data = buf + body;
            dataLen = size <= avail ? size : avail; // tolerate truncated files / bogus sizes
        }
        if (size > avail) break;
        pos = body + size + (size & 1); // chunks are padded to even size
    }

    if (format < 0) { snprintf(err, errLen, "chunk fmt mancante"); return false; }
    if (!data || dataLen == 0) { snprintf(err, errLen, "nessun dato audio"); return false; }
    bool isFloat = format == 3;
    if (format != 1 && !(isFloat && bits == 32)) {
        snprintf(err, errLen, "formato %d non supportato (solo PCM)", format);
        return false;
    }
    if (bits != 8 && bits != 16 && bits != 24 && bits != 32) {
        snprintf(err, errLen, "%d bit non supportati", bits);
        return false;
    }
    if (channels < 1 || channels > 8) { snprintf(err, errLen, "%d canali non supportati", channels); return false; }
    if (rate < 8000 || rate > 192000) { snprintf(err, errLen, "sample rate %u non supportato", (unsigned)rate); return false; }
    int frameBytes = channels * (bits / 8);
    if (blockAlign < frameBytes) blockAlign = frameBytes;

    uint64_t srcFrames = dataLen / (size_t)blockAlign;
    uint64_t maxFrames = (uint64_t)maxSeconds * rate;
    if (maxSeconds > 0 && srcFrames > maxFrames) srcFrames = maxFrames;
    if (srcFrames == 0) { snprintf(err, errLen, "file audio vuoto"); return false; }

    uint64_t outFrames = srcFrames * AUDIO_RATE / rate;
    if (outFrames == 0) outFrames = 1;
    int16_t* o = (int16_t*)malloc((size_t)outFrames * 4);
    if (!o) { snprintf(err, errLen, "memoria insufficiente"); return false; }

    int bps = bits / 8;
    // decode frame i to (l, r)
    auto frame = [&](uint64_t i, int* l, int* r) {
        const unsigned char* p = data + i * (uint64_t)blockAlign;
        *l = read_sample(p, bits, isFloat);
        *r = channels >= 2 ? read_sample(p + bps, bits, isFloat) : *l;
    };

    if (rate == AUDIO_RATE) {
        for (uint64_t i = 0; i < outFrames; i++) {
            int l, r;
            frame(i, &l, &r);
            o[i * 2] = (int16_t)l; o[i * 2 + 1] = (int16_t)r;
        }
    } else {
        // linear interpolation, 32.32 fixed-point source position
        uint64_t step = ((uint64_t)rate << 32) / AUDIO_RATE;
        uint64_t posFx = 0;
        for (uint64_t i = 0; i < outFrames; i++, posFx += step) {
            uint64_t idx = posFx >> 32;
            int frac = (int)((posFx >> 16) & 0xFFFF);
            if (idx >= srcFrames) idx = srcFrames - 1;
            uint64_t nxt = idx + 1 < srcFrames ? idx + 1 : idx;
            int l0, r0, l1, r1;
            frame(idx, &l0, &r0);
            frame(nxt, &l1, &r1);
            o[i * 2]     = (int16_t)(l0 + (((l1 - l0) * frac) >> 16));
            o[i * 2 + 1] = (int16_t)(r0 + (((r1 - r0) * frac) >> 16));
        }
    }
    out->data = o;
    out->frames = (uint32_t)outFrames;
    return true;
}

bool wav_load(const char* path, Sound* out, char* err, int errLen, uint32_t maxSeconds) {
    unsigned char* buf = nullptr;
    size_t len = 0;
    if (!fs_read_file(path, WAV_MAX_FILE, &buf, &len)) {
        snprintf(err, errLen, "impossibile leggere il file (mancante o > 128 MB)");
        return false;
    }
    bool ok = wav_parse(buf, len, out, err, errLen, maxSeconds);
    free(buf);
    return ok;
}
