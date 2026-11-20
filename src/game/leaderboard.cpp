#include "leaderboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../platform/fs.h"

static const char* MAGIC = "NEXTBOT_LB,1";
static const char* FILE_NAME = "leaderboard.dat";
static const size_t MAX_FILE = 256 * 1024;
static const uint32_t MAX_MS = 100u * 3600u * 1000u; // 100 hours: anything above is garbage

static LbRecord s_rec[LB_MAX_RECORDS];
static int s_count = 0;

void leaderboard_sanitize(char* dst, int dstLen, const char* src) {
    if (dstLen <= 0) return;
    int o = 0;
    for (const unsigned char* p = (const unsigned char*)(src ? src : ""); *p && o < dstLen - 1; p++) {
        unsigned char c = *p;
        if (c < 32 || c == 127 || c == ',' || c == '"' || c == ';') c = '_';
        dst[o++] = (char)c;
    }
    // do not leave half of a multi-byte UTF-8 character at the end
    int end = o;
    while (end > 0 && ((unsigned char)dst[end - 1] & 0xC0) == 0x80) end--;
    if (end > 0 && ((unsigned char)dst[end - 1] & 0x80)) {
        unsigned char lead = (unsigned char)dst[end - 1];
        int need = (lead >= 0xF0) ? 4 : (lead >= 0xE0) ? 3 : (lead >= 0xC0) ? 2 : 1;
        if (o - (end - 1) < need) o = end - 1;
    }
    dst[o] = 0;
}

void format_survival_time(uint32_t ms, char* buf, int len) {
    uint32_t s = ms / 1000;
    if (s < 60) snprintf(buf, len, "%u sec", (unsigned)s);
    else snprintf(buf, len, "%u min %u sec", (unsigned)(s / 60), (unsigned)(s % 60));
}

static int cmp_rec(const void* a, const void* b) {
    const LbRecord* x = (const LbRecord*)a;
    const LbRecord* y = (const LbRecord*)b;
    if (x->ms != y->ms) return x->ms > y->ms ? -1 : 1; // longest survival first
    return strcmp(x->png, y->png);
}

static void sort_records() { qsort(s_rec, s_count, sizeof(LbRecord), cmp_rec); }

static bool lb_path(char* buf, int len) { return fs_path(buf, len, FILE_NAME); }

// Parses "user,png,ms". Returns false for malformed lines.
static bool parse_line(char* line, LbRecord* r) {
    char* c1 = strchr(line, ',');
    if (!c1) return false;
    char* c2 = strchr(c1 + 1, ',');
    if (!c2 || strchr(c2 + 1, ',')) return false;
    *c1 = 0; *c2 = 0;
    const char* num = c2 + 1;
    if (!*num) return false;
    uint64_t v = 0;
    for (const char* p = num; *p; p++) {
        if (*p < '0' || *p > '9') return false;
        v = v * 10 + (uint64_t)(*p - '0');
        if (v > MAX_MS) return false;
    }
    if (!line[0] || !c1[1]) return false;
    leaderboard_sanitize(r->user, LB_USER_MAX, line);
    leaderboard_sanitize(r->png, LB_PNG_MAX, c1 + 1);
    r->ms = (uint32_t)v;
    return true;
}

void leaderboard_load() {
    s_count = 0;
    char path[FS_PATH_MAX];
    if (!lb_path(path, sizeof(path))) return;
    unsigned char* data = nullptr;
    size_t len = 0;
    if (!fs_read_file(path, MAX_FILE, &data, &len)) return; // missing or unreadable: start empty
    char* text = (char*)data;                                // fs_read_file zero-terminates
    bool first = true;
    char* next = text;
    while (next && *next) {
        // split off one line (handles \n and \r\n), skip empty ones
        char* line = next;
        char* end = strpbrk(line, "\r\n");
        if (end) { *end = 0; next = end + 1; } else next = nullptr;
        if (!line[0]) continue;
        if (first) {
            first = false;
            if (strcmp(line, MAGIC) != 0) { printf("[nextbot] leaderboard: bad header, ignoring file\n"); break; }
            continue;
        }
        if (s_count >= LB_MAX_RECORDS) break;
        LbRecord r;
        if (!parse_line(line, &r)) continue;
        // keep one record per PNG (the best), even if the file has duplicates
        int dup = -1;
        for (int i = 0; i < s_count; i++) if (strcmp(s_rec[i].png, r.png) == 0) { dup = i; break; }
        if (dup >= 0) { if (r.ms > s_rec[dup].ms) s_rec[dup] = r; }
        else s_rec[s_count++] = r;
    }
    free(data);
    sort_records();
}

bool leaderboard_save() {
    char path[FS_PATH_MAX];
    if (!lb_path(path, sizeof(path))) return false;
    size_t cap = 32 + (size_t)s_count * (LB_USER_MAX + LB_PNG_MAX + 16);
    char* buf = (char*)malloc(cap);
    if (!buf) return false;
    size_t n = (size_t)snprintf(buf, cap, "%s\n", MAGIC);
    for (int i = 0; i < s_count && n < cap; i++)
        n += (size_t)snprintf(buf + n, cap - n, "%s,%s,%u\n", s_rec[i].user, s_rec[i].png, (unsigned)s_rec[i].ms);
    if (n > cap) n = cap;
    bool ok = fs_write_atomic(path, buf, n);
    free(buf);
    if (!ok) printf("[nextbot] leaderboard: save failed (%s)\n", path);
    return ok;
}

const LbRecord* leaderboard_find(const char* png) {
    char key[LB_PNG_MAX];
    leaderboard_sanitize(key, sizeof(key), png);
    for (int i = 0; i < s_count; i++) if (strcmp(s_rec[i].png, key) == 0) return &s_rec[i];
    return nullptr;
}

bool leaderboard_submit(const char* user, const char* png, uint32_t ms) {
    if (ms > MAX_MS) ms = MAX_MS;
    LbRecord r;
    leaderboard_sanitize(r.user, LB_USER_MAX, (user && user[0]) ? user : "Player");
    leaderboard_sanitize(r.png, LB_PNG_MAX, png);
    if (!r.png[0]) return false;
    r.ms = ms;
    LbRecord* existing = (LbRecord*)leaderboard_find(r.png);
    if (existing) {
        if (ms <= existing->ms) return false;
        *existing = r;
    } else {
        if (s_count >= LB_MAX_RECORDS) {
            // full: replace the worst record only if this one is better
            sort_records();
            if (ms <= s_rec[s_count - 1].ms) return false;
            s_rec[s_count - 1] = r;
        } else {
            s_rec[s_count++] = r;
        }
    }
    sort_records();
    leaderboard_save();
    return true;
}

int leaderboard_count() { return s_count; }
const LbRecord* leaderboard_get(int i) { return (i >= 0 && i < s_count) ? &s_rec[i] : nullptr; }

bool leaderboard_reset() {
    s_count = 0;
    memset(s_rec, 0, sizeof(s_rec));
    char path[FS_PATH_MAX], tmp[FS_PATH_MAX];
    if (!lb_path(path, sizeof(path))) return false;
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    fs_remove(tmp);
    return fs_remove(path);
}
