#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bot.h"
#include "catalog.h"
#include "map.h"
#include "pathfind.h"
#include "player.h"
#include "settings.h"
#include "ui.h"
#include "view3d.h"
#include "../gfx/font.h"
#include "../gfx/sprite.h"
#include "../platform/audio.h"
#include "../platform/input.h"

static const int BOT_TEXTURE = 256;    // billboard texture (resized once at load time)
static const int BIG_SPRITE = 470;     // jumpscare size
static const float BOT_HEIGHT = 0.95f; // world units (walls are 1.0)
static const uint32_t WAV_MAX_SECONDS = 600;

// ---- session state ----
static Map s_map;
static Player s_player;
static Bot s_bot;
static int s_botIndex = -1, s_diff = DIFF_MEDIUM, s_mapIndex = 0;
static char s_name[FS_NAME_MAX];
static Sprite s_botSprite, s_bigSprite;
static Sound s_sound;
static bool s_hasSound = false;
static char s_warning[128];
static float s_time = 0;            // survival time (seconds, pauses excluded)
static float s_banner = 0;          // "SCAPPA!" banner timer
static bool s_paused = false;
static int s_pauseSel = 0;
static float s_dist = 99;
static float s_side = 0;            // bot direction relative to the view, -1 left .. 1 right
static float s_light = 1;           // neon flicker
static float s_flickerTimer = 0, s_flickerLeft = 0;
static uint32_t s_rng = 12345;

static uint32_t hash2(int x, int y, int seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float frand() { s_rng = s_rng * 1103515245u + 12345u; return ((s_rng >> 8) & 0xFFFF) / 65536.0f; }

// ================================================================ session ===
static void reset_positions() {
    player_init(&s_player, s_map.spawnX + 0.5f, s_map.spawnY + 0.5f,
                player_open_direction(s_map, s_map.spawnX, s_map.spawnY));
    int fx, fy;
    if (!pathfind_farthest(s_map, s_map.spawnX, s_map.spawnY, &fx, &fy)) { fx = s_map.spawnX; fy = s_map.spawnY; }
    bot_init(&s_bot, fx + 0.5f, fy + 0.5f, difficulty_base_speed(s_diff));
    s_time = 0;
    s_banner = 2.5f;
    s_paused = false;
    s_pauseSel = 0;
    s_dist = 99;
    s_side = 0;
    s_light = 1;
    s_flickerTimer = 3.0f;
    s_flickerLeft = 0;
}

bool game_start(int botIndex, int difficulty, int mapIndex, char* err, int errLen) {
    game_end();
    const BotEntry* entry = catalog_get(botIndex);
    if (!entry) { snprintf(err, errLen, "Bot non trovato"); return false; }
    if (!map_load(mapIndex, &s_map)) { snprintf(err, errLen, "Mappa non valida"); return false; }
    s_botIndex = botIndex; s_diff = difficulty; s_mapIndex = mapIndex;
    snprintf(s_name, sizeof(s_name), "%s", entry->name);
    s_warning[0] = 0;

    // PNG: decode once, keep only the two resized copies
    char path[FS_PATH_MAX], why[96];
    Sprite full;
    if (!catalog_png_path(botIndex, path, sizeof(path)) || !sprite_load_png(path, &full, why, sizeof(why))) {
        snprintf(err, errLen, "%s", entry->error[0] ? entry->error : "Impossibile leggere il PNG");
        return false;
    }
    bool ok = sprite_resize_fit(full, BOT_TEXTURE, BOT_TEXTURE, &s_botSprite) &&
              sprite_resize_fit(full, BIG_SPRITE, BIG_SPRITE, &s_bigSprite);
    sprite_free(&full);
    if (!ok) { game_end(); snprintf(err, errLen, "Memoria insufficiente per lo sprite"); return false; }

    // WAV: optional, a broken file just makes the bot silent
    s_hasSound = false;
    if (entry->hasSound && catalog_wav_path(botIndex, path, sizeof(path))) {
        if (wav_load(path, &s_sound, why, sizeof(why), WAV_MAX_SECONDS)) s_hasSound = true;
        else { snprintf(s_warning, sizeof(s_warning), "Audio non caricato: %s", why); printf("[nextbot] %s: %s\n", path, why); }
    }

    view3d_init(s_map.theme);
    reset_positions();
    if (s_hasSound) {
        audio_set_loop_gain(bot_volume_for_distance(99), 0);
        audio_set_loop(&s_sound);
    }
    return true;
}

void game_restart() {
    reset_positions();
    if (s_hasSound) audio_set_loop(&s_sound); // restart the loop from the beginning
}

void game_end() {
    audio_set_loop(nullptr); // returns once the audio thread has let go of the buffer
    sound_free(&s_sound);
    s_hasSound = false;
    sprite_free(&s_botSprite);
    sprite_free(&s_bigSprite);
    input_set_feedback(0x100820, 0, 0);
}

void game_shutdown() {
    game_end();
    view3d_free();
}

uint32_t game_survival_ms() { return (uint32_t)(s_time * 1000.0f); }
const char* game_bot_name() { return s_name; }

void game_debug_set_bot(float x, float y) { s_bot.x = x; s_bot.y = y; }
float game_debug_distance() { return s_dist; }
void game_debug_positions(float* px, float* py, float* bx, float* by) { *px = s_player.x; *py = s_player.y; *bx = s_bot.x; *by = s_bot.y; }
float game_debug_angle() { return s_player.angle; }
void game_debug_set_angle(float a) { s_player.angle = a; }

static void update_feedback() {
    // lightbar: dim yellow far away, red when close; rumble only when very close
    float near = 1.0f - s_dist / 10.0f;
    if (near < 0) near = 0;
    if (near > 1) near = 1;
    int r = (int)(40 + near * 215), g = (int)(30 * (1 - near)), b = (int)(6 * (1 - near));
    uint8_t rumble = s_dist < 3.0f ? (uint8_t)((3.0f - s_dist) / 3.0f * 180.0f) : 0;
    input_set_feedback(((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b, rumble, rumble / 2);
}

// Neon lights: mostly steady, sometimes a short flicker; more often when the bot is near.
static void update_flicker(float dt) {
    if (s_flickerLeft > 0) {
        s_flickerLeft -= dt;
        s_light = frand() < 0.5f ? 0.45f : 0.8f;
        if (s_flickerLeft <= 0) s_light = 1;
        return;
    }
    s_flickerTimer -= dt * (s_dist < 8 ? 3.0f : 1.0f);
    if (s_flickerTimer <= 0) {
        s_flickerLeft = 0.12f + frand() * 0.3f;
        s_flickerTimer = 2.0f + frand() * 6.0f;
    }
}

GameEvent game_update(const Controls& c, float dt) {
    if (!c.connected && !s_paused) { s_paused = true; s_pauseSel = 0; } // controller lost: pause
    if (c.pressed & PAD_OPTIONS) {
        s_paused = !s_paused;
        s_pauseSel = 0;
        audio_play_sfx(s_paused ? SFX_CONFIRM : SFX_BACK);
    }
    if (s_paused) {
        if (c.pressed & (PAD_UP | PAD_DOWN)) { s_pauseSel ^= 1; audio_play_sfx(SFX_MOVE); }
        if (c.pressed & PAD_CIRCLE) { s_paused = false; audio_play_sfx(SFX_BACK); }
        else if (c.pressed & PAD_CROSS) {
            if (s_pauseSel == 1) return GAME_QUIT;
            s_paused = false;
            audio_play_sfx(SFX_CONFIRM);
        }
        audio_set_loop_gain(0.04f, 0);
        input_set_feedback(0x202020, 0, 0);
        return GAME_RUNNING;
    }

    s_time += dt;
    if (s_banner > 0) s_banner -= dt;

    // left stick: forward/back + strafe; right stick: turn. D-pad: up/down walk, left/right turn.
    float forward = -c.ly, strafe = c.lx, turn = c.rx * g_settings.lookSpeed;
    if (c.lx == 0 && c.ly == 0) {
        if (c.held & PAD_UP) forward += 1;
        if (c.held & PAD_DOWN) forward -= 1;
    }
    // (the left stick is also folded into LEFT/RIGHT by controls: that is strafing, not turning)
    if (c.rx == 0 && c.lx == 0) {
        if (c.held & (PAD_LEFT | PAD_L1)) turn -= 1;
        if (c.held & (PAD_RIGHT | PAD_R1)) turn += 1;
    }
    bool sprint = (c.held & (PAD_R2 | PAD_SQUARE)) || c.r2 > 0.3f;
    player_update(&s_player, s_map, forward, strafe, turn, sprint, dt);
    bot_update(&s_bot, s_map, s_player.x, s_player.y, dt);

    float dx = s_bot.x - s_player.x, dy = s_bot.y - s_player.y;
    s_dist = sqrtf(dx * dx + dy * dy);
    // which side is the bot on? dot product with the view's right-hand vector
    float rx = -sinf(s_player.angle), ry = cosf(s_player.angle);
    s_side = s_dist > 0.01f ? (dx * rx + dy * ry) / s_dist : 0;
    audio_set_loop_gain(bot_volume_for_distance(s_dist), bot_pan_for_side(s_side));
    update_feedback();
    update_flicker(dt);
    return s_dist < BOT_CATCH_RADIUS ? GAME_CAUGHT : GAME_RUNNING;
}

// ================================================================ rendering ===
static void format_clock(float t, char* buf, int len) {
    int tenths = (int)(t * 10.0f);
    snprintf(buf, len, "%02d:%02d.%d", tenths / 600, (tenths / 10) % 60, tenths % 10);
}

static void render_hud(Canvas& c) {
    char buf[96];
    format_clock(s_time, buf, sizeof(buf));
    font_draw_centered(c, c.w / 2, 14, buf, ui::COL_TEXT, 4);

    // bot (top-left)
    const BotEntry* e = catalog_get(s_botIndex);
    draw_rect_a(c, 10, 10, 280, 66, 0xFF000000, 140);
    if (e && e->thumb.px) sprite_blit(c, e->thumb, 15 + (THUMB_SIZE - e->thumb.w) / 2, 15 + (THUMB_SIZE - e->thumb.h) / 2);
    char name[32];
    ui::ellipsize(name, sizeof(name), s_name, 13);
    font_draw_shadow(c, 80, 20, name, ui::COL_DANGER, 2);
    snprintf(buf, sizeof(buf), "%s - %s", difficulty_name(s_diff), s_hasSound ? "audio" : "muto");
    font_draw_shadow(c, 80, 48, buf, ui::COL_DIM, 1);

    // record (top-right)
    const LbRecord* rec = leaderboard_find(s_name);
    if (rec) {
        char t[32];
        format_survival_time(rec->ms, t, sizeof(t));
        snprintf(buf, sizeof(buf), "Record: %s", t);
    } else snprintf(buf, sizeof(buf), "Record: nessuno");
    bool beating = rec && s_time * 1000.0f > rec->ms;
    font_draw_shadow(c, c.w - font_width(buf, 2) - 16, 20, buf, beating ? ui::COL_GOOD : ui::COL_TEXT, 2);
    if (beating) font_draw_shadow(c, c.w - font_width("Stai battendo il record!", 1) - 16, 46, "Stai battendo il record!", ui::COL_GOOD, 1);

    // stamina (bottom-left)
    int bx = 16, by = c.h - 40, bw = 220;
    font_draw_shadow(c, bx, by - 18, "SCATTO (R2 / Quadrato)", ui::COL_DIM, 1);
    draw_rect(c, bx, by, bw, 16, rgb(30, 28, 24));
    draw_rect(c, bx + 2, by + 2, (int)((bw - 4) * s_player.stamina), 12, s_player.exhausted ? ui::COL_DANGER : ui::COL_ACCENT);

    if (s_banner > 0) {
        int a = s_banner > 0.5f ? 255 : (int)(s_banner / 0.5f * 255);
        font_draw_centered(c, c.w / 2, c.h / 2 - 110, "SCAPPA!", blend(0xFF000000, ui::COL_DANGER, (uint32_t)a), 6);
        snprintf(buf, sizeof(buf), "Mappa: %s", s_map.name);
        font_draw_centered(c, c.w / 2, c.h / 2 - 50, buf, blend(0xFF000000, ui::COL_TEXT, (uint32_t)a), 2);
        font_draw_centered(c, c.w / 2, c.h / 2 - 20, "Levetta sinistra: muoviti - Levetta destra: guardati intorno",
                           blend(0xFF000000, ui::COL_TEXT, (uint32_t)a), 1);
    }
    if (s_warning[0] && s_time < 5.0f) font_draw_centered(c, c.w / 2, c.h - 90, s_warning, ui::COL_DANGER, 1);
}

// Dark vignette around the edges, red and pulsing when the bot is close.
static void render_vignette(Canvas& c, float time, float near) {
    int pulse = (int)(near * (90 + 50 * sinf(time * 12.0f)));
    const int band = 14;
    for (int i = 0; i < 6; i++) {
        int a = 70 - i * 11;
        int pa = pulse * (6 - i) / 6;
        for (int k = 0; k < 2; k++) {
            uint32_t col = k == 0 ? 0xFF000000u : rgb(200, 0, 0);
            int al = k == 0 ? a : pa;
            if (al <= 0) continue;
            draw_rect_a(c, 0, i * band, c.w, band, col, al);
            draw_rect_a(c, 0, c.h - (i + 1) * band, c.w, band, col, al);
            draw_rect_a(c, i * band, (i + 1) * band, band, c.h - 2 * (i + 1) * band, col, al);
            draw_rect_a(c, c.w - (i + 1) * band, (i + 1) * band, band, c.h - 2 * (i + 1) * band, col, al);
        }
    }
}

void game_render(Canvas& c, float time) {
    float near = 1.0f - s_dist / 6.0f;
    if (near < 0) near = 0;
    Camera cam;
    cam.x = s_player.x; cam.y = s_player.y;
    cam.angle = s_player.angle;
    cam.bob = sinf(s_player.walkPhase) * 2.5f * s_player.moveAmount;
    if (!s_paused && near > 0.3f) { // the view shakes when it is right behind you
        float amp = (near - 0.3f) * 6.0f;
        cam.bob += ((int)(hash2((int)(time * 60), 1, 9) % 100) / 100.0f - 0.5f) * amp;
        cam.angle += ((int)(hash2((int)(time * 60), 2, 9) % 100) / 100.0f - 0.5f) * amp * 0.004f;
    }
    Billboard bb;
    bb.sprite = &s_botSprite;
    bb.x = s_bot.x; bb.y = s_bot.y;
    bb.height = BOT_HEIGHT;
    bb.lift = 0.04f + 0.03f * sinf(time * 4.0f);
    view3d_render(c, s_map, cam, &bb, s_paused ? 1.0f : s_light);
    render_vignette(c, time, s_paused ? 0 : near);
    render_hud(c);

    if (s_paused) {
        draw_rect_a(c, 0, 0, c.w, c.h, 0xFF000000, 160);
        int w = 400, h = 200, x = (c.w - w) / 2, y = (c.h - h) / 2;
        ui::panel(c, x, y, w, h);
        font_draw_centered(c, c.w / 2, y + 22, "PAUSA", ui::COL_ACCENT, 4);
        const char* items[2] = { "Riprendi", "Torna al menu" };
        for (int i = 0; i < 2; i++) {
            int iy = y + 90 + i * 46;
            if (i == s_pauseSel) ui::selection_bar(c, x + 20, iy - 6, w - 40, 36, time);
            font_draw_centered(c, c.w / 2, iy + 2, items[i], i == s_pauseSel ? ui::COL_ACCENT : ui::COL_TEXT, 2);
        }
        ui::Hint hh[] = { { PAD_CROSS, "Conferma" }, { PAD_CIRCLE, "Riprendi" } };
        ui::hints(c, hh, 2);
    }
}

// =============================================================== game over ===
static float s_goTime = 0;
static bool s_goNew = false, s_goHadPrev = false, s_goSfx = false;
static LbRecord s_goPrev;
static const float GO_SCARE = 1.3f; // jumpscare duration before the results show up

void gameover_open(bool newRecord, const LbRecord* prevBest) {
    s_goTime = 0;
    s_goNew = newRecord;
    s_goHadPrev = prevBest != nullptr;
    if (prevBest) s_goPrev = *prevBest;
    s_goSfx = false;
    audio_play_sfx(SFX_CAUGHT);
}

GameOverResult gameover_update(const Controls& c, float dt) {
    s_goTime += dt;
    // the bot screams at full volume, then fades out
    float vol = s_goTime < 1.0f ? 1.0f : 1.0f - (s_goTime - 1.0f) / 1.5f;
    audio_set_loop_gain(vol > 0 ? vol : 0, 0);
    bool flash = ((int)(s_goTime * 8.0f)) & 1;
    input_set_feedback(s_goTime < GO_SCARE ? (flash ? 0xFF0000 : 0x400000) : 0x500000, s_goTime < 0.6f ? 255 : 0, s_goTime < 0.6f ? 200 : 0);
    if (s_goNew && !s_goSfx && s_goTime >= GO_SCARE) { audio_play_sfx(SFX_RECORD); s_goSfx = true; }
    if (s_goTime < GO_SCARE) return GO_STAY;
    if (c.pressed & PAD_CROSS) { audio_play_sfx(SFX_CONFIRM); return GO_MENU; }
    if (c.pressed & PAD_TRIANGLE) { audio_play_sfx(SFX_CONFIRM); return GO_RETRY; }
    return GO_STAY;
}

void gameover_render(Canvas& c, float time) {
    if (s_goTime < GO_SCARE) {
        // jumpscare: flashing background, the bot filling the screen and shaking
        bool flash = ((int)(s_goTime * 14.0f)) & 1;
        draw_clear(c, flash ? rgb(160, 0, 0) : rgb(10, 0, 0));
        int amp = (int)(14 * (1.0f - s_goTime / GO_SCARE)) + 2;
        int jx = (int)(hash2((int)(time * 60), 5, 1) % (uint32_t)(amp * 2 + 1)) - amp;
        int jy = (int)(hash2((int)(time * 60), 6, 1) % (uint32_t)(amp * 2 + 1)) - amp;
        if (flash) sprite_blit_tint(c, s_bigSprite, (c.w - s_bigSprite.w) / 2 + jx, (c.h - s_bigSprite.h) / 2 + jy, rgb(255, 0, 0), 90);
        else sprite_blit(c, s_bigSprite, (c.w - s_bigSprite.w) / 2 + jx, (c.h - s_bigSprite.h) / 2 + jy);
        return;
    }
    draw_gradient_v(c, 0, 0, c.w, c.h, rgb(60, 0, 0), rgb(8, 0, 0));
    sprite_blit(c, s_bigSprite, (440 - s_bigSprite.w) / 2, (c.h - s_bigSprite.h) / 2 - 20, 150); // backdrop, left of the panel

    int px = 440, pw = c.w - px - 30, py = 70, ph = 380;
    ui::panel(c, px, py, pw, ph);
    int cx = px + pw / 2;
    font_draw_centered(c, cx, py + 24, "TI HA PRESO!", ui::COL_DANGER, 4);
    char name[40], buf[96], t[32];
    ui::ellipsize(name, sizeof(name), s_name, 24);
    font_draw_centered(c, cx, py + 80, name, ui::COL_TEXT, 2);
    font_draw_centered(c, cx, py + 130, "Sei sopravvissuto:", ui::COL_DIM, 2);
    format_survival_time(game_survival_ms(), t, sizeof(t));
    font_draw_centered(c, cx, py + 160, t, ui::COL_ACCENT, 4);
    if (s_goNew) {
        if (((int)(time * 3.0f)) & 1) font_draw_centered(c, cx, py + 230, "NUOVO RECORD!", ui::COL_GOOD, 4);
        if (s_goHadPrev) {
            format_survival_time(s_goPrev.ms, t, sizeof(t));
            snprintf(buf, sizeof(buf), "Precedente: %s", t);
            font_draw_centered(c, cx, py + 290, buf, ui::COL_DIM, 2);
        }
    } else if (s_goHadPrev) {
        char who[24];
        format_survival_time(s_goPrev.ms, t, sizeof(t));
        ui::ellipsize(who, sizeof(who), s_goPrev.user, 12);
        snprintf(buf, sizeof(buf), "Record: %s - %s", who, t);
        font_draw_centered(c, cx, py + 240, buf, ui::COL_TEXT, 2);
    }
    ui::Hint h[] = { { PAD_CROSS, "Torna al menu" }, { PAD_TRIANGLE, "Riprova" } };
    ui::hints(c, h, 2);
}
