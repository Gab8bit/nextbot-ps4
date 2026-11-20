// PC test harness for Nextbot (Linux, built with "make host-test" inside the build container).
// Runs unit tests on the portable modules, then drives the real app state machine with
// scripted pad input and saves screenshots as PNG. Also generates sce_sys/icon0.png.
//
//   nextbot_host <outdir>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"

#include "game/app.h"
#include "game/bot.h"
#include "game/catalog.h"
#include "game/game.h"
#include "game/leaderboard.h"
#include "game/map.h"
#include "game/pathfind.h"
#include "game/player.h"
#include "game/view3d.h"
#include "gfx/draw.h"
#include "gfx/font.h"
#include "gfx/sprite.h"
#include "platform/audio.h"
#include "platform/fs.h"
#include "platform/wav.h"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) g_pass++; else { g_fail++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char g_out[512];
static uint32_t g_pixels[SCREEN_W * SCREEN_H];
static Canvas g_canvas = { g_pixels, SCREEN_W, SCREEN_H };

// Writes 24-bit RGB PNGs (no alpha): Sony's checker rejects 32-bit icon0/pic1 files.
static void save_png(const char* name, const uint32_t* px, int w, int h) {
    unsigned char* rgb = (unsigned char*)malloc((size_t)w * h * 3);
    for (int i = 0; i < w * h; i++) {
        uint32_t p = px[i];
        rgb[i * 3] = (p >> 16) & 255; rgb[i * 3 + 1] = (p >> 8) & 255; rgb[i * 3 + 2] = p & 255;
    }
    char path[700];
    if (name[0] == '/' || strchr(name, '/')) snprintf(path, sizeof(path), "%s", name);
    else snprintf(path, sizeof(path), "%s/%s", g_out, name);
    stbi_write_png(path, w, h, 3, rgb, w * 3);
    free(rgb);
}

static void shot(const char* name) { save_png(name, g_pixels, SCREEN_W, SCREEN_H); }

// ------------------------------------------------------------- test assets ---
static void write_file(const char* path, const void* data, size_t len) {
    FILE* f = fopen(path, "wb");
    if (!f) { printf("cannot write %s\n", path); return; }
    fwrite(data, 1, len, f);
    fclose(f);
}

// A cartoon face with transparent background (RGBA), w x h.
static void make_face_png(const char* path, int w, int h, uint32_t skin) {
    unsigned char* p = (unsigned char*)calloc((size_t)w * h, 4);
    float cx = w / 2.0f, cy = h / 2.0f, r = (w < h ? w : h) * 0.45f;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float dx = x - cx, dy = y - cy, d = sqrtf(dx * dx + dy * dy);
            unsigned char* q = p + ((size_t)y * w + x) * 4;
            if (d > r) continue;
            uint32_t c = skin;
            // eyes
            float ex = fabsf(dx) - r * 0.35f, ey = dy + r * 0.25f;
            if (ex * ex + ey * ey < (r * 0.14f) * (r * 0.14f)) c = 0x000000;
            // grin
            if (dy > r * 0.2f && dy < r * 0.45f && fabsf(dx) < r * 0.55f) c = 0x400000;
            if (dy > r * 0.2f && dy < r * 0.27f && fabsf(dx) < r * 0.55f && ((int)(dx + r) / 6) % 2) c = 0xFFFFFF;
            q[0] = (c >> 16) & 255; q[1] = (c >> 8) & 255; q[2] = c & 255; q[3] = d > r - 2 ? 128 : 255;
        }
    stbi_write_png(path, w, h, 4, p, w * 4);
    free(p);
}

// Builds a WAV in memory. fmtTag 1 = PCM, 3 = float, 0xFFFE = extensible(PCM).
static size_t make_wav(unsigned char* out, int fmtTag, int channels, int rate, int bits, int frames, bool extraChunk) {
    int bps = bits / 8, block = channels * bps;
    size_t dataLen = (size_t)frames * block;
    size_t pos = 0;
    auto w32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) out[pos++] = (v >> (8 * i)) & 255; };
    auto w16 = [&](uint16_t v) { out[pos++] = v & 255; out[pos++] = v >> 8; };
    auto tag = [&](const char* t) { memcpy(out + pos, t, 4); pos += 4; };
    bool ext = fmtTag == 0xFFFE;
    tag("RIFF"); w32(0); tag("WAVE");
    if (extraChunk) { tag("LIST"); w32(3); out[pos++] = 'a'; out[pos++] = 'b'; out[pos++] = 'c'; out[pos++] = 0; } // odd size + pad
    tag("fmt "); w32(ext ? 40 : 16);
    w16((uint16_t)fmtTag); w16((uint16_t)channels); w32((uint32_t)rate); w32((uint32_t)(rate * block)); w16((uint16_t)block); w16((uint16_t)bits);
    if (ext) { w16(22); w16((uint16_t)bits); w32(3); w16(1); for (int i = 0; i < 14; i++) out[pos++] = 0; }
    tag("data"); w32((uint32_t)dataLen);
    for (int f = 0; f < frames; f++)
        for (int ch = 0; ch < channels; ch++) {
            float v = (ch == 0 ? 0.5f : -0.25f) * sinf(f * 0.01f) + (ch == 0 ? 0.25f : -0.25f);
            if (fmtTag == 3) { float fv = v; memcpy(out + pos, &fv, 4); pos += 4; }
            else if (bits == 8) out[pos++] = (unsigned char)(128 + (int)(v * 127));
            else if (bits == 16) w16((uint16_t)(int16_t)(v * 32767));
            else if (bits == 24) { int s = (int)(v * 8388607); out[pos++] = s & 255; out[pos++] = (s >> 8) & 255; out[pos++] = (s >> 16) & 255; }
            else if (bits == 32) w32((uint32_t)(int32_t)(v * 2147483647.0));
        }
    uint32_t riff = (uint32_t)(pos - 8);
    memcpy(out + 4, &riff, 4);
    return pos;
}

static void make_assets(const char* root) {
    char p[600];
    mkdir(root, 0777);
    snprintf(p, sizeof(p), "%s/png", root); mkdir(p, 0777);
    snprintf(p, sizeof(p), "%s/sounds", root); mkdir(p, 0777);
    snprintf(p, sizeof(p), "%s/png/smiley.png", root); make_face_png(p, 640, 480, 0xF2D33A);
    snprintf(p, sizeof(p), "%s/png/Gigante,Verde.PNG", root); make_face_png(p, 1200, 1500, 0x5DBB4A);
    snprintf(p, sizeof(p), "%s/png/piccolo.png", root); make_face_png(p, 24, 24, 0xE05050);
    snprintf(p, sizeof(p), "%s/png/rotto.png", root); write_file(p, "not a png at all", 16);
    snprintf(p, sizeof(p), "%s/png/._smiley.png", root); write_file(p, "mac junk", 8);
    // sounds: smiley.wav (44.1 kHz stereo 16-bit), GIGANTE,VERDE.wav (case-insensitive match, 8-bit mono)
    static unsigned char buf[2 * 1024 * 1024];
    size_t n = make_wav(buf, 1, 2, 44100, 16, 44100, true);
    snprintf(p, sizeof(p), "%s/sounds/smiley.wav", root); write_file(p, buf, n);
    n = make_wav(buf, 1, 1, 22050, 8, 22050, false);
    snprintf(p, sizeof(p), "%s/sounds/GIGANTE,VERDE.wav", root); write_file(p, buf, n);
    snprintf(p, sizeof(p), "%s/sounds/orfano.wav", root); write_file(p, buf, n);
}

// ------------------------------------------------------------- unit tests ---
static void test_format() {
    char b[32];
    format_survival_time(0, b, sizeof(b));       CHECK(strcmp(b, "0 sec") == 0, "%s", b);
    format_survival_time(59999, b, sizeof(b));   CHECK(strcmp(b, "59 sec") == 0, "%s", b);
    format_survival_time(60000, b, sizeof(b));   CHECK(strcmp(b, "1 min 0 sec") == 0, "%s", b);
    format_survival_time(126400, b, sizeof(b));  CHECK(strcmp(b, "2 min 6 sec") == 0, "%s", b);
    char s[8];
    leaderboard_sanitize(s, sizeof(s), "a,b\nc\"d");  CHECK(strcmp(s, "a_b_c_d") == 0, "%s", s);
    leaderboard_sanitize(s, 6, "abcd\xC3\xA8");        CHECK(strcmp(s, "abcd") == 0, "utf8 cut '%s'", s); // è would be split
    leaderboard_sanitize(s, 7, "abcd\xC3\xA8");        CHECK(strcmp(s, "abcd\xC3\xA8") == 0, "utf8 keep '%s'", s);
}

static void test_leaderboard(const char* root) {
    fs_set_root(root);
    mkdir(root, 0777);
    char path[600];
    fs_path(path, sizeof(path), "leaderboard.dat");
    unlink(path);
    leaderboard_load();
    CHECK(leaderboard_count() == 0, "missing file -> empty");

    CHECK(leaderboard_submit("Mario", "shrek", 126000), "first record is new");
    CHECK(!leaderboard_submit("Luigi", "shrek", 100000), "worse time is not a record");
    CHECK(leaderboard_submit("Luigi", "shrek", 130000), "better time is a record");
    CHECK(leaderboard_submit("Peach,Toad", "bot,one", 5000), "record with commas");
    CHECK(leaderboard_count() == 2, "two pngs -> two records (%d)", leaderboard_count());
    CHECK(strcmp(leaderboard_get(0)->png, "shrek") == 0 && strcmp(leaderboard_get(0)->user, "Luigi") == 0, "sorted best first");
    CHECK(fs_exists(path), "file written");
    char tmp[620]; snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    CHECK(!fs_exists(tmp), "no temp file left behind");

    leaderboard_load();
    CHECK(leaderboard_count() == 2, "reload keeps 2 (%d)", leaderboard_count());
    const LbRecord* r = leaderboard_find("bot,one");
    CHECK(r && strcmp(r->user, "Peach_Toad") == 0 && r->ms == 5000, "sanitized record survives reload");

    // corrupt content: wrong header -> empty, no crash
    write_file(path, "garbage\n\x01\x02,,,\n", 15);
    leaderboard_load();
    CHECK(leaderboard_count() == 0, "corrupt header -> empty");
    // empty file
    write_file(path, "", 0);
    leaderboard_load();
    CHECK(leaderboard_count() == 0, "empty file -> empty");
    // good header, mixed good and bad lines, CRLF, duplicates
    const char* mixed = "NEXTBOT_LB,1\r\nA,x,1000\r\nbad line\r\nB,y,abc\r\nC,z,99999999999999\r\nD,x,3000\r\n,,5\r\nE,w,7000";
    write_file(path, mixed, strlen(mixed));
    leaderboard_load();
    CHECK(leaderboard_count() == 2, "mixed file -> 2 valid unique records (%d)", leaderboard_count());
    r = leaderboard_find("x");
    CHECK(r && r->ms == 3000 && strcmp(r->user, "D") == 0, "duplicate keeps the best");

    CHECK(leaderboard_reset(), "reset ok");
    CHECK(leaderboard_count() == 0 && !fs_exists(path), "reset empties and deletes the file");
    leaderboard_load();
    CHECK(leaderboard_count() == 0, "after reset load -> empty");
    CHECK(leaderboard_reset(), "reset without file is fine");
}

static void test_wav() {
    static unsigned char buf[4 * 1024 * 1024];
    struct Case { int tag, ch, rate, bits; bool extra; } cases[] = {
        { 1, 2, 44100, 16, true }, { 1, 1, 22050, 8, false }, { 1, 2, 48000, 24, false },
        { 3, 1, 32000, 32, false }, { 0xFFFE, 2, 96000, 16, false }, { 1, 6, 48000, 16, false }, { 1, 1, 8000, 32, false },
    };
    for (auto& c : cases) {
        size_t n = make_wav(buf, c.tag, c.ch, c.rate, c.bits, c.rate / 2, c.extra); // 0.5 s
        Sound s; char err[128] = "";
        bool ok = wav_parse(buf, n, &s, err, sizeof(err), 60);
        CHECK(ok, "wav tag=%d ch=%d rate=%d bits=%d: %s", c.tag, c.ch, c.rate, c.bits, err);
        if (!ok) continue;
        CHECK(abs((int)s.frames - AUDIO_RATE / 2) <= 2, "resampled length %u", s.frames);
        // frame 0: L = 0.25, R = -0.25 (stereo) or L = R = 0.25 (mono)
        float l = s.data[0] / 32767.0f, r = s.data[1] / 32767.0f;
        CHECK(fabsf(l - 0.25f) < 0.02f, "left sample %f (bits %d)", l, c.bits);
        CHECK(fabsf(r - (c.ch >= 2 ? -0.25f : 0.25f)) < 0.02f, "right sample %f", r);
        sound_free(&s);
    }
    Sound s; char err[128];
    CHECK(!wav_parse((const unsigned char*)"RIFF", 4, &s, err, sizeof(err), 60), "too short rejected");
    size_t n = make_wav(buf, 1, 2, 44100, 16, 1000, false);
    CHECK(!wav_parse(buf, 30, &s, err, sizeof(err), 60), "truncated header rejected: %s", err);
    buf[20] = 2; // ADPCM
    CHECK(!wav_parse(buf, n, &s, err, sizeof(err), 60), "compressed format rejected");
    n = make_wav(buf, 1, 2, 44100, 16, 44100, false);
    CHECK(wav_parse(buf, n - 1001, &s, err, sizeof(err), 60), "truncated data tolerated");
    sound_free(&s);
    n = make_wav(buf, 1, 1, 48000, 16, 48000, false);
    CHECK(wav_parse(buf, n, &s, err, sizeof(err), 0) && s.frames == 48000, "maxSeconds 0 = no limit");
    sound_free(&s);
}

static void test_maps() {
    for (int i = 0; i < map_count(); i++) {
        Map m;
        bool ok = map_load(i, &m);
        CHECK(ok, "map %d loads", i);
        if (!ok) continue;
        int freeTiles = 0, walled = 0;
        const MapDef* d = map_def(i);
        for (int y = 0; y < m.h; y++)
            for (int x = 0; x < m.w; x++) {
                if (!m.tiles[y * m.w + x]) freeTiles++;
                else if (d->rows[y][x] == '0' && x > 0 && y > 0 && x < m.w - 1 && y < m.h - 1) walled++;
            }
        printf("  map %s: %dx%d, %d free tiles, %d unreachable tiles filled\n", m.name, m.w, m.h, freeTiles, walled);
        CHECK(walled == 0, "map %s has %d unreachable free tiles (design issue)", m.name, walled);
        int fx, fy;
        CHECK(pathfind_farthest(m, m.spawnX, m.spawnY, &fx, &fy), "farthest");
        static int path[BOT_PATH_MAX];
        int len = pathfind_astar(m, fx, fy, m.spawnX, m.spawnY, path, BOT_PATH_MAX);
        CHECK(len > 10, "A* path from bot spawn to player: %d", len);
        // path continuity
        bool cont = true;
        for (int k = 1; k < len; k++) {
            int ax = path[k - 1] % m.w, ay = path[k - 1] / m.w, bx = path[k] % m.w, by = path[k] / m.w;
            if (abs(ax - bx) > 1 || abs(ay - by) > 1 || map_wall(m, bx, by)) cont = false;
        }
        CHECK(cont, "A* path is continuous and on free tiles");
        float spawnDist = sqrtf((float)((fx - m.spawnX) * (fx - m.spawnX) + (fy - m.spawnY) * (fy - m.spawnY)));
        CHECK(spawnDist > 12, "bot spawns far from player (%.1f tiles)", spawnDist);

        // A still player gets caught; the bot must never end up inside a wall.
        Bot b;
        bot_init(&b, fx + 0.5f, fy + 0.5f, difficulty_base_speed(DIFF_MEDIUM));
        float px = m.spawnX + 0.5f, py = m.spawnY + 0.5f, t = 0;
        bool inWall = false;
        while (t < 60) {
            bot_update(&b, m, px, py, 1 / 60.0f);
            t += 1 / 60.0f;
            if (map_box_hits_wall(m, b.x, b.y, 0.2f)) inWall = true;
            if (hypotf(b.x - px, b.y - py) < BOT_CATCH_RADIUS) break;
        }
        CHECK(t < 60, "bot reaches a still player (%.1f s)", t);
        CHECK(!inWall, "bot never clips into walls");
        // one search every BOT_REPATH_INTERVAL (rounded up to whole frames), never one per frame
        CHECK(b.repaths >= (int)(t / (BOT_REPATH_INTERVAL + 1 / 60.0f)) - 1 && b.repaths <= (int)(t / BOT_REPATH_INTERVAL) + 2,
              "A* runs at intervals, not every frame (%d searches in %.1f s)", b.repaths, t);
        printf("  map %s: medium bot catches a still player in %.1f s\n", m.name, t);

        // player collision: push into every direction for a while, never inside a wall
        Player p;
        player_init(&p, m.spawnX + 0.5f, m.spawnY + 0.5f, player_open_direction(m, m.spawnX, m.spawnY));
        CHECK(!map_wall(m, (int)(p.x + cosf(p.angle) * 2), (int)(p.y + sinf(p.angle) * 2)), "player starts facing open space");
        bool clip = false;
        for (int k = 0; k < 3000; k++) {
            float a = k * 0.013f;
            player_update(&p, m, cosf(a), sinf(a * 1.3f), sinf(a * 0.7f), k % 200 < 100, 1 / 60.0f);
            if (map_box_hits_wall(m, p.x, p.y, PLAYER_HALF - 0.01f)) clip = true;
        }
        CHECK(!clip, "player never overlaps walls");
    }
    // volume curve
    CHECK(bot_volume_for_distance(1000) > 0.05f, "minimum volume > 0");
    CHECK(bot_volume_for_distance(1) > bot_volume_for_distance(5) && bot_volume_for_distance(5) > bot_volume_for_distance(15), "closer = louder");
}

static void test_sprite() {
    Sprite s;
    sprite_create(&s, 100, 50);
    for (int i = 0; i < 5000; i++) s.px[i] = (i % 2) ? 0xFFFF0000 : 0x00000000;
    Sprite t;
    CHECK(sprite_resize_fit(s, 20, 20, &t) && t.w == 20 && t.h == 10, "fit keeps aspect %dx%d", t.w, t.h);
    uint32_t p = t.px[0];
    CHECK(((p >> 16) & 255) == 255 && (p & 0xFFFF) == 0, "premultiplied box filter keeps pure color: %08x", p);
    sprite_free(&t);
    CHECK(sprite_resize_fit(s, 400, 400, &t) && t.w == 400 && t.h == 200, "upscale %dx%d", t.w, t.h);
    sprite_free(&t);
    sprite_free(&s);
}

// --------------------------------------------------------- app simulation ---
static PadRaw g_pad;
static void frames(int n, uint32_t buttons = 0, float lx = 0, float ly = 0, float rx = 0) {
    for (int i = 0; i < n; i++) {
        memset(&g_pad, 0, sizeof(g_pad));
        g_pad.connected = true;
        g_pad.buttons = buttons;
        g_pad.lx = lx; g_pad.ly = ly; g_pad.rx = rx;
        app_frame(g_pad, 1 / 60.0f, g_canvas);
        static int16_t audio[800 * 2];
        audio_mix(audio, 800); // one frame of audio at 48 kHz
    }
}
static void press(uint32_t b) { frames(1, b); frames(1); }

static float audio_level() {
    static int16_t buf[4800 * 2];
    audio_mix(buf, 4800);
    double sum = 0;
    for (int i = 0; i < 4800 * 2; i++) sum += fabs((double)buf[i]);
    return (float)(sum / (4800 * 2));
}

static void simulate(const char* root) {
    fs_set_root(root);
    make_assets(root);
    audio_init();
    app_init("Mario");
    frames(1); shot("01_loading.png");
    frames(2);
    CHECK(app_state() == ST_MENU, "boot -> menu");
    CHECK(catalog_count() == 4, "4 bots found (%d)", catalog_count());
    for (int i = 0; i < catalog_count(); i++) {
        const BotEntry* b = catalog_get(i);
        printf("  bot %d: '%s' file '%s' sound '%s' thumb %dx%d err '%s'\n", i, b->name, b->pngFile, b->wavFile, b->thumb.w, b->thumb.h, b->error);
    }
    const BotEntry* g = catalog_get(0);
    CHECK(g && strcmp(g->name, "Gigante,Verde") == 0 && g->hasSound, "case-insensitive wav match");
    CHECK(catalog_get(1) && strcmp(catalog_get(1)->name, "piccolo") == 0 && !catalog_get(1)->hasSound, "bot without sound");
    CHECK(catalog_get(2) && !catalog_get(2)->thumb.px && catalog_get(2)->error[0], "broken png flagged");
    frames(30); shot("02_menu.png");

    press(PAD_CROSS);
    CHECK(app_state() == ST_SELECT, "menu -> select");
    frames(10); shot("03_select.png");
    // broken PNG shows an error
    press(PAD_DOWN); press(PAD_DOWN);
    press(PAD_CROSS);
    frames(5); shot("04_select_error.png");
    CHECK(app_state() == ST_SELECT, "broken png does not start");
    press(PAD_CROSS);
    press(PAD_DOWN); // smiley
    press(PAD_RIGHT); // fast
    frames(5); shot("05_select_smiley.png");
    frames(1, PAD_CROSS); CHECK(app_state() == ST_LOADING, "loading");
    frames(1); shot("05b_loading_bot.png");
    frames(1); CHECK(app_state() == ST_PLAYING, "playing");
    frames(30); shot("06_play_start.png");
    float px, py, bx, by;
    Map map0;
    map_load(0, &map0);
    game_debug_positions(&px, &py, &bx, &by);
    CHECK(!map_line_clear(map0, px, py, bx, by, 0.05f) && !view3d_bot_visible(), "bot far behind walls is not visible");
    float far = audio_level();
    // walk and look around for a while
    frames(150, PAD_R2, 0.0f, -1.0f);
    frames(60, 0, 0.0f, 0.0f, 0.8f);
    frames(120, 0, 0.0f, -1.0f);
    shot("07_play_running.png");
    game_debug_positions(&px, &py, &bx, &by);
    printf("  after 6 s: player (%.1f, %.1f) bot (%.1f, %.1f) dist %.1f\n", px, py, bx, by, game_debug_distance());
    // pause
    press(PAD_OPTIONS);
    frames(10); shot("08_pause.png");
    press(PAD_OPTIONS);
    // look down the longest free direction and put the bot there: far, then close
    float look = player_open_direction(map0, (int)px, (int)py);
    game_debug_positions(&px, &py, &bx, &by);
    px = (int)px + 0.5f; py = (int)py + 0.5f;
    int run = 0;
    while (!map_wall(map0, (int)(px + cosf(look) * (run + 1)), (int)(py + sinf(look) * (run + 1)))) run++;
    printf("  free run ahead: %d tiles\n", run);
    // teleport the player to the tile centre facing that way (via the debug hooks)
    game_debug_set_angle(look);
    game_debug_set_bot(px + cosf(look) * (run > 1 ? run - 0.5f : 1.5f), py + sinf(look) * (run > 1 ? run - 0.5f : 1.5f));
    frames(2);
    shot("08b_play_corridor.png");
    game_debug_positions(&px, &py, &bx, &by);
    look = game_debug_angle();
    game_debug_set_bot(px + cosf(look) * 1.6f, py + sinf(look) * 1.6f);
    frames(2);
    CHECK(view3d_bot_visible(), "bot right in front is visible");
    shot("09_play_close.png");
    float near = audio_level();
    printf("  audio level far %.0f, close %.0f\n", far, near);
    CHECK(near > far * 1.5f && far > 0, "bot louder when closer (far %.0f, close %.0f)", far, near);
    // let it catch us
    int guard = 0;
    while (app_state() == ST_PLAYING && guard++ < 600) frames(1);
    CHECK(app_state() == ST_GAME_OVER, "caught -> game over");
    frames(20); shot("10_gameover_scare.png");
    frames(90); shot("11_gameover.png");
    const LbRecord* r = leaderboard_find("smiley");
    CHECK(r && strcmp(r->user, "Mario") == 0, "record saved for smiley");
    // retry, then die immediately: not a new record
    press(PAD_TRIANGLE);
    CHECK(app_state() == ST_PLAYING, "retry");
    game_debug_positions(&px, &py, &bx, &by);
    game_debug_set_bot(px, py);
    frames(2);
    CHECK(app_state() == ST_GAME_OVER, "instant catch");
    frames(100); shot("12_gameover_norecord.png");
    press(PAD_CROSS);
    CHECK(app_state() == ST_SELECT, "game over -> select");
    press(PAD_CIRCLE);
    CHECK(app_state() == ST_MENU, "select -> menu");

    // leaderboard
    press(PAD_DOWN); press(PAD_CROSS);
    CHECK(app_state() == ST_LEADERBOARD, "menu -> leaderboard");
    leaderboard_submit("Luigi", "Gigante,Verde", 61000);
    leaderboard_submit("Una persona dal nome molto lungo", "un_png_con_un_nome_davvero_lunghissimo_che_non_entra", 7000);
    frames(5); shot("13_leaderboard.png");
    press(PAD_CIRCLE);
    CHECK(app_state() == ST_MENU, "leaderboard -> menu");

    // settings + reset with confirmation
    press(PAD_DOWN); press(PAD_CROSS);
    CHECK(app_state() == ST_SETTINGS, "menu -> settings");
    press(PAD_LEFT); // volume 70 %
    CHECK(fabsf(audio_get_master() - 0.7f) < 0.01f, "volume setting applies (%.2f)", audio_get_master());
    frames(5); shot("14_settings.png");
    press(PAD_DOWN); press(PAD_DOWN); press(PAD_DOWN); press(PAD_CROSS); // -> "Resetta classifica"
    frames(5); shot("15_settings_confirm.png");
    press(PAD_CROSS); // default is "No"
    CHECK(leaderboard_count() == 3, "default No keeps the leaderboard (%d)", leaderboard_count());
    press(PAD_CROSS); press(PAD_LEFT); press(PAD_CROSS); // open, select Yes, confirm
    frames(5); shot("16_settings_done.png");
    char lbPath[600]; fs_path(lbPath, sizeof(lbPath), "leaderboard.dat");
    CHECK(leaderboard_count() == 0 && !fs_exists(lbPath), "reset clears records and file");
    frames(200);
    press(PAD_CIRCLE);
    CHECK(app_state() == ST_MENU, "settings -> menu");
    app_shutdown();
}

static void simulate_empty(const char* root) {
    // folders do not exist yet: the app must create them and explain where to put files
    fs_set_root(root);
    app_init("");
    frames(3);
    char p[600]; snprintf(p, sizeof(p), "%s/png", root);
    CHECK(fs_is_dir(p), "png folder created automatically");
    snprintf(p, sizeof(p), "%s/sounds", root);
    CHECK(fs_is_dir(p), "sounds folder created automatically");
    press(PAD_CROSS);
    frames(5); shot("17_no_bots.png");
    press(PAD_CROSS); // nothing to start
    CHECK(app_state() == ST_SELECT, "no bots: stays in select");
    app_shutdown();
}

// Placeholder icon: 512x512.
static void make_icon(const char* path) {
    static uint32_t px[512 * 512];
    Canvas c = { px, 512, 512 };
    draw_gradient_v(c, 0, 0, 512, 512, rgb(70, 8, 8), rgb(10, 0, 0));
    draw_ellipse_a(c, 256, 220, 160, 160, rgb(0, 0, 0), 120);
    draw_ellipse_a(c, 256, 210, 150, 150, rgb(240, 214, 70), 255);
    draw_ellipse_a(c, 200, 170, 26, 34, rgb(0, 0, 0), 255);
    draw_ellipse_a(c, 312, 170, 26, 34, rgb(0, 0, 0), 255);
    draw_ellipse_a(c, 206, 162, 7, 9, rgb(255, 255, 255), 255);
    draw_ellipse_a(c, 318, 162, 7, 9, rgb(255, 255, 255), 255);
    draw_rect(c, 160, 250, 192, 50, rgb(60, 0, 0));
    for (int i = 0; i < 8; i++) draw_rect(c, 164 + i * 24, 252, 18, 18, rgb(250, 250, 240));
    font_draw_shadow(c, 256 - font_width("NEXTBOT", 8) / 2, 405, "NEXTBOT", rgb(255, 211, 77), 8);
    save_png(path, px, 512, 512);
}

// Background shown behind the app tile on the PS4 home screen: 1920x1080, a first-person
// view of the Backrooms map with a bot at the end of the corridor.
static void make_pic1(const char* path, const char* botPng) {
    Map m;
    if (!map_load(0, &m)) return;
    view3d_init(THEME_BACKROOMS);
    Sprite full, bot;
    char err[96];
    bool haveBot = sprite_load_png(botPng, &full, err, sizeof(err)) && sprite_resize_fit(full, 256, 256, &bot);
    Camera cam = { m.spawnX + 0.5f, m.spawnY + 0.5f, player_open_direction(m, m.spawnX, m.spawnY), 0 };
    Billboard bb = { &bot, cam.x + cosf(cam.angle) * 5.0f, cam.y + sinf(cam.angle) * 5.0f, 0.95f, 0.05f };
    view3d_render(g_canvas, m, cam, haveBot ? &bb : nullptr, 1.0f);
    draw_rect_a(g_canvas, 0, 0, SCREEN_W, 120, 0xFF000000, 120);
    font_draw_centered(g_canvas, SCREEN_W / 2, 26, "NEXTBOT", rgb(255, 211, 77), 8);
    static uint32_t big[1920 * 1080];
    for (int y = 0; y < 1080; y++)
        for (int x = 0; x < 1920; x++) big[y * 1920 + x] = g_pixels[(y / 2) * SCREEN_W + x / 2];
    save_png(path, big, 1920, 1080);
    if (haveBot) { sprite_free(&full); sprite_free(&bot); }
}

int main(int argc, char** argv) {
    snprintf(g_out, sizeof(g_out), "%s", argc > 1 ? argv[1] : "build/host");
    mkdir(g_out, 0777);
    char root[600];

    printf("== unit tests\n");
    test_format();
    snprintf(root, sizeof(root), "%s/lbtest", g_out);
    test_leaderboard(root);
    test_wav();
    test_sprite();
    test_maps();

    printf("== app simulation\n");
    snprintf(root, sizeof(root), "%s/data", g_out);
    simulate(root);
    snprintf(root, sizeof(root), "%s/empty", g_out);
    simulate_empty(root);

    make_icon("sce_sys/icon0.png");
    snprintf(root, sizeof(root), "%s/data/png/smiley.png", g_out);
    make_pic1("sce_sys/pic1.png", root);
    audio_shutdown();
    printf("== %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
