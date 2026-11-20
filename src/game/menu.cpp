#include "menu.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bot.h"
#include "catalog.h"
#include "leaderboard.h"
#include "map.h"
#include "settings.h"
#include "ui.h"
#include "../gfx/font.h"
#include "../platform/audio.h"
#include "../platform/fs.h"

// =============================================================== main menu ===
static int s_mainSel = 0;
static const char* MAIN_ITEMS[3] = { "Gioca", "Classifica", "Impostazioni" };

void mainmenu_open(bool resetCursor) { if (resetCursor) s_mainSel = 0; }

MainChoice mainmenu_update(const Controls& c) {
    if (c.pressed & PAD_UP)   { s_mainSel = (s_mainSel + 2) % 3; audio_play_sfx(SFX_MOVE); }
    if (c.pressed & PAD_DOWN) { s_mainSel = (s_mainSel + 1) % 3; audio_play_sfx(SFX_MOVE); }
    if (c.pressed & PAD_CROSS) {
        audio_play_sfx(SFX_CONFIRM);
        return s_mainSel == 0 ? MAIN_PLAY : s_mainSel == 1 ? MAIN_LEADERBOARD : MAIN_SETTINGS;
    }
    return MAIN_NONE;
}

void mainmenu_render(Canvas& c, float time, const char* userName) {
    ui::background(c, time);
    // title with a nervous jitter
    int jx = (int)(sinf(time * 37.0f) * 2.0f * (fmodf(time, 3.0f) < 0.15f ? 3 : 0));
    font_draw_centered(c, c.w / 2 + jx + 4, 70 + 4, "NEXTBOT", rgb(120, 0, 0), 9);
    font_draw_centered(c, c.w / 2 + jx, 70, "NEXTBOT", ui::COL_ACCENT, 9);
    font_draw_centered(c, c.w / 2, 158, "SCAPPA. NON FERMARTI.", ui::COL_DANGER, 2);

    int w = 360, rowH = 58, y0 = 240, x = (c.w - w) / 2;
    ui::panel(c, x, y0 - 20, w, 3 * rowH + 30);
    for (int i = 0; i < 3; i++) {
        int y = y0 + i * rowH;
        if (i == s_mainSel) ui::selection_bar(c, x + 10, y - 4, w - 20, rowH - 10, time);
        font_draw_centered(c, c.w / 2, y + 8, MAIN_ITEMS[i], i == s_mainSel ? ui::COL_ACCENT : ui::COL_TEXT, 3);
    }
    char who[64];
    snprintf(who, sizeof(who), "Giocatore: %s", userName);
    font_draw_shadow(c, 20, 20, who, ui::COL_DIM, 2);
    ui::Hint h[] = { { PAD_UP, "Muovi" }, { PAD_CROSS, "Conferma" } };
    ui::hints(c, h, 2);
}

// ============================================================== bot select ===
static int s_bot = 0, s_scroll = 0, s_diff = DIFF_MEDIUM, s_map = 0;
static char s_error[160];
static const int VISIBLE_ROWS = 5, ROW_H = 72;

void botselect_open(bool resetCursor) {
    if (resetCursor) { s_bot = 0; s_scroll = 0; s_diff = g_settings.difficulty; }
    if (s_bot >= catalog_count()) s_bot = catalog_count() > 0 ? catalog_count() - 1 : 0;
    s_error[0] = 0;
}

void botselect_set_error(const char* msg) { snprintf(s_error, sizeof(s_error), "%s", msg); }
int botselect_bot() { return s_bot; }
int botselect_difficulty() { return s_diff; }
int botselect_map() { return s_map; }

SelectResult botselect_update(const Controls& c) {
    if (s_error[0]) {
        if (c.pressed & (PAD_CROSS | PAD_CIRCLE)) s_error[0] = 0;
        return SELECT_STAY;
    }
    int n = catalog_count();
    if (c.pressed & PAD_CIRCLE) { audio_play_sfx(SFX_BACK); return SELECT_BACK; }
    if (c.pressed & PAD_TRIANGLE) { audio_play_sfx(SFX_CONFIRM); return SELECT_RESCAN; }
    if (n == 0) return SELECT_STAY;
    if (c.pressed & PAD_UP)    { s_bot = (s_bot + n - 1) % n; audio_play_sfx(SFX_MOVE); }
    if (c.pressed & PAD_DOWN)  { s_bot = (s_bot + 1) % n; audio_play_sfx(SFX_MOVE); }
    if (c.pressed & PAD_LEFT)  { s_diff = (s_diff + DIFF_COUNT - 1) % DIFF_COUNT; audio_play_sfx(SFX_MOVE); }
    if (c.pressed & PAD_RIGHT) { s_diff = (s_diff + 1) % DIFF_COUNT; audio_play_sfx(SFX_MOVE); }
    if (c.pressed & PAD_L1)    { s_map = (s_map + map_count() - 1) % map_count(); audio_play_sfx(SFX_MOVE); }
    if (c.pressed & PAD_R1)    { s_map = (s_map + 1) % map_count(); audio_play_sfx(SFX_MOVE); }
    if (s_bot < s_scroll) s_scroll = s_bot;
    if (s_bot >= s_scroll + VISIBLE_ROWS) s_scroll = s_bot - VISIBLE_ROWS + 1;
    if (c.pressed & PAD_CROSS) {
        const BotEntry* b = catalog_get(s_bot);
        if (!b || !b->thumb.px) {
            audio_play_sfx(SFX_BACK);
            botselect_set_error(b && b->error[0] ? b->error : "Immagine non valida");
            return SELECT_STAY;
        }
        audio_play_sfx(SFX_CONFIRM);
        return SELECT_START;
    }
    return SELECT_STAY;
}

static void render_no_bots(Canvas& c) {
    char png[FS_PATH_MAX], snd[FS_PATH_MAX];
    snprintf(png, sizeof(png), "%s/png/", fs_root());
    snprintf(snd, sizeof(snd), "%s/sounds/", fs_root());
    int x = 90, w = c.w - 180, y = 120;
    ui::panel(c, x, y, w, 300);
    font_draw_centered(c, c.w / 2, y + 22, "Nessun bot trovato!", ui::COL_DANGER, 3);
    font_draw_centered(c, c.w / 2, y + 74, "Copia le immagini PNG nella cartella:", ui::COL_TEXT, 2);
    font_draw_centered(c, c.w / 2, y + 100, png, ui::COL_ACCENT, 2);
    font_draw_centered(c, c.w / 2, y + 138, "e i suoni WAV (stesso nome) in:", ui::COL_TEXT, 2);
    font_draw_centered(c, c.w / 2, y + 164, snd, ui::COL_ACCENT, 2);
    font_draw_centered(c, c.w / 2, y + 204, "Esempio: png/shrek.png + sounds/shrek.wav", ui::COL_DIM, 2);
    if (!catalog_folders_ok())
        font_draw_centered(c, c.w / 2, y + 240, "Attenzione: impossibile creare le cartelle, crearle via FTP.", ui::COL_DANGER, 2);
    else
        font_draw_centered(c, c.w / 2, y + 240, "Copiali via FTP o USB, poi premi Triangolo.", ui::COL_DIM, 2);
}

void botselect_render(Canvas& c, float time) {
    ui::background(c, time);
    ui::title(c, "SCEGLI IL BOT", 24);
    int n = catalog_count();
    if (n == 0) {
        render_no_bots(c);
        ui::Hint h[] = { { PAD_TRIANGLE, "Ricarica" }, { PAD_CIRCLE, "Indietro" } };
        ui::hints(c, h, 2);
        return;
    }

    // ---- list (left) ----
    int lx = 30, lw = 520, ly = 96;
    ui::panel(c, lx, ly, lw, VISIBLE_ROWS * ROW_H + 16);
    for (int r = 0; r < VISIBLE_ROWS; r++) {
        int i = s_scroll + r;
        if (i >= n) break;
        const BotEntry* b = catalog_get(i);
        int y = ly + 8 + r * ROW_H;
        if (i == s_bot) ui::selection_bar(c, lx + 6, y, lw - 12, ROW_H - 4, time);
        draw_rect(c, lx + 18, y + 4, THUMB_SIZE + 4, THUMB_SIZE + 4, rgb(40, 36, 30));
        if (b->thumb.px)
            sprite_blit(c, b->thumb, lx + 20 + (THUMB_SIZE - b->thumb.w) / 2, y + 6 + (THUMB_SIZE - b->thumb.h) / 2);
        else
            font_draw(c, lx + 20 + THUMB_SIZE / 2 - 8, y + 6 + THUMB_SIZE / 2 - 8, "?", ui::COL_DANGER, 2);
        char name[40];
        ui::ellipsize(name, sizeof(name), b->name, 22);
        font_draw_shadow(c, lx + 92, y + 14, name, i == s_bot ? ui::COL_ACCENT : ui::COL_TEXT, 2);
        font_draw_shadow(c, lx + 92, y + 40, !b->thumb.px ? "immagine non valida" : b->hasSound ? "con audio" : "senza audio (muto)",
                         !b->thumb.px ? ui::COL_DANGER : b->hasSound ? ui::COL_GOOD : ui::COL_DIM, 1);
    }
    // scroll indicators
    if (s_scroll > 0) draw_triangle(c, lx + lw / 2.0f, ly - 10.0f, lx + lw / 2.0f - 10, ly - 2.0f, lx + lw / 2.0f + 10, ly - 2.0f, ui::COL_ACCENT);
    int bottom = ly + VISIBLE_ROWS * ROW_H + 16;
    if (s_scroll + VISIBLE_ROWS < n) draw_triangle(c, lx + lw / 2.0f, bottom + 10.0f, lx + lw / 2.0f - 10, bottom + 2.0f, lx + lw / 2.0f + 10, bottom + 2.0f, ui::COL_ACCENT);
    char count[32];
    snprintf(count, sizeof(count), "%d/%d", s_bot + 1, n);
    font_draw_shadow(c, lx + lw - font_width(count, 1) - 8, bottom + 6, count, ui::COL_DIM, 1);

    // ---- details (right) ----
    const BotEntry* b = catalog_get(s_bot);
    int rx = 570, rw = c.w - rx - 30, ry = 96, rh = VISIBLE_ROWS * ROW_H + 16;
    ui::panel(c, rx, ry, rw, rh);
    int bob = (int)(sinf(time * 3.0f) * 4.0f);
    if (b->preview.px)
        sprite_blit(c, b->preview, rx + (rw - b->preview.w) / 2, ry + 14 + (PREVIEW_SIZE - b->preview.h) / 2 + bob);
    int ty = ry + PREVIEW_SIZE + 26;
    char line[96];
    const LbRecord* rec = leaderboard_find(b->name);
    if (rec) {
        char t[32];
        format_survival_time(rec->ms, t, sizeof(t));
        char who[24];
        ui::ellipsize(who, sizeof(who), rec->user, 10);
        snprintf(line, sizeof(line), "Record: %s (%s)", t, who);
    } else {
        snprintf(line, sizeof(line), "Record: nessuno");
    }
    font_draw_centered(c, rx + rw / 2, ty, line, ui::COL_TEXT, 1);
    snprintf(line, sizeof(line), "Velocità: < %s >", difficulty_name(s_diff));
    font_draw_centered(c, rx + rw / 2, ty + 26, line, ui::COL_ACCENT, 2);
    snprintf(line, sizeof(line), "Mappa: L1 %s R1", map_def(s_map)->name);
    font_draw_centered(c, rx + rw / 2, ty + 56, line, ui::COL_ACCENT, 2);

    ui::Hint h[] = { { PAD_CROSS, "Gioca" }, { PAD_LEFT, "Velocità" }, { PAD_L1, "Mappa" },
                     { PAD_TRIANGLE, "Ricarica" }, { PAD_CIRCLE, "Indietro" } };
    ui::hints(c, h, 5);

    if (s_error[0]) ui::message_box(c, "Impossibile avviare", s_error, "Premi X per continuare", ui::COL_DANGER);
}

// ============================================================ leaderboard ===
static int s_lbScroll = 0;
static const int LB_ROWS = 10;

void lbscreen_open() { s_lbScroll = 0; }

bool lbscreen_update(const Controls& c) {
    int n = leaderboard_count();
    int maxScroll = n > LB_ROWS ? n - LB_ROWS : 0;
    if ((c.pressed & PAD_UP) && s_lbScroll > 0) { s_lbScroll--; audio_play_sfx(SFX_MOVE); }
    if ((c.pressed & PAD_DOWN) && s_lbScroll < maxScroll) { s_lbScroll++; audio_play_sfx(SFX_MOVE); }
    if (c.pressed & (PAD_CIRCLE | PAD_CROSS)) { audio_play_sfx(SFX_BACK); return true; }
    return false;
}

void lbscreen_render(Canvas& c, float time) {
    ui::background(c, time);
    ui::title(c, "CLASSIFICA", 24);
    int n = leaderboard_count();
    int x = 60, w = c.w - 120, y0 = 96, rowH = 34;
    ui::panel(c, x, y0, w, LB_ROWS * rowH + 50);
    font_draw_shadow(c, x + 20, y0 + 12, "Miglior tempo per ogni bot, dal più lungo", ui::COL_DIM, 1);
    if (n == 0) {
        font_draw_centered(c, c.w / 2, y0 + 150, "Ancora nessun record.", ui::COL_TEXT, 2);
        font_draw_centered(c, c.w / 2, y0 + 180, "Gioca una partita per entrare in classifica!", ui::COL_DIM, 2);
    }
    for (int r = 0; r < LB_ROWS; r++) {
        int i = s_lbScroll + r;
        const LbRecord* rec = leaderboard_get(i);
        if (!rec) break;
        int y = y0 + 34 + r * rowH;
        if (r % 2 == 0) draw_rect_a(c, x + 8, y - 4, w - 16, rowH, rgb(255, 255, 255), 10);
        // "Mario | shrek | 2 min 6 sec"
        char t[32], user[40], png[64], line[160];
        format_survival_time(rec->ms, t, sizeof(t));
        ui::ellipsize(user, sizeof(user), rec->user, 14);
        ui::ellipsize(png, sizeof(png), rec->png, 20);
        snprintf(line, sizeof(line), "%s | %s | %s", user, png, t);
        char rank[8];
        snprintf(rank, sizeof(rank), "%d.", i + 1);
        uint32_t col = i == 0 ? ui::COL_ACCENT : i < 3 ? rgb(230, 200, 140) : ui::COL_TEXT;
        font_draw_shadow(c, x + 20, y + 4, rank, col, 2);
        font_draw_shadow(c, x + 84, y + 4, line, col, 2);
    }
    if (n > LB_ROWS) {
        char more[48];
        snprintf(more, sizeof(more), "%d-%d di %d", s_lbScroll + 1, s_lbScroll + LB_ROWS, n);
        font_draw_shadow(c, x + w - font_width(more, 1) - 12, y0 + 12, more, ui::COL_DIM, 1);
    }
    ui::Hint h[] = { { PAD_UP, "Scorri" }, { PAD_CIRCLE, "Indietro" } };
    ui::hints(c, h, 2);
}
