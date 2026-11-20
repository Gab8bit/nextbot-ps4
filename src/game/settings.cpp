#include "settings.h"

#include <stdio.h>
#include "bot.h"
#include "leaderboard.h"
#include "ui.h"
#include "../gfx/font.h"
#include "../platform/audio.h"

AppSettings g_settings = { 0.8f, DIFF_MEDIUM, 1.0f };

// ---- items -----------------------------------------------------------------
// Each item can describe its value, react to left/right and/or to X.
// To add a setting, add a row to ITEMS with the callbacks it needs.
struct SettingItem {
    const char* label;
    void (*value)(char* buf, int len);  // nullptr = no value shown
    void (*change)(int dir);            // left/right, nullptr = not adjustable
    void (*activate)();                 // X, nullptr = nothing
};

static void vol_value(char* b, int l) { snprintf(b, l, "< %d%% >", (int)(g_settings.volume * 100 + 0.5f)); }
static void vol_change(int d) {
    float v = g_settings.volume + d * 0.1f;
    g_settings.volume = v < 0 ? 0 : (v > 1 ? 1 : v);
    audio_set_master(g_settings.volume);
}
static void diff_value(char* b, int l) { snprintf(b, l, "< %s >", difficulty_name(g_settings.difficulty)); }
static void diff_change(int d) { g_settings.difficulty = (g_settings.difficulty + d + DIFF_COUNT) % DIFF_COUNT; }
static void look_value(char* b, int l) { snprintf(b, l, "< %d%% >", (int)(g_settings.lookSpeed * 100 + 0.5f)); }
static void look_change(int d) {
    float v = g_settings.lookSpeed + d * 0.1f;
    g_settings.lookSpeed = v < 0.5f ? 0.5f : (v > 2.0f ? 2.0f : v);
}
static void reset_activate();

static const SettingItem ITEMS[] = {
    { "Volume generale",       vol_value,  vol_change,  nullptr },
    { "Difficoltà predefinita", diff_value, diff_change, nullptr },
    { "Sensibilità visuale",   look_value, look_change, nullptr },
    { "Resetta classifica",    nullptr,    nullptr,     reset_activate },
};
static const int ITEM_COUNT = (int)(sizeof(ITEMS) / sizeof(ITEMS[0]));

// ---- state -------------------------------------------------------------------
static int s_sel = 0;
static bool s_confirm = false;     // confirmation dialog open
static bool s_confirmYes = false;  // highlighted choice (defaults to No)
static float s_msgTimer = 0;
static bool s_msgOk = true;

static void reset_activate() {
    s_confirm = true;
    s_confirmYes = false; // safe default
}

void settings_open() {
    s_sel = 0;
    s_confirm = false;
    s_msgTimer = 0;
}

SettingsResult settings_update(const Controls& c, float dt) {
    if (s_msgTimer > 0) {
        s_msgTimer -= dt;
        if (c.pressed & (PAD_CROSS | PAD_CIRCLE)) s_msgTimer = 0;
        return SETTINGS_STAY;
    }
    if (s_confirm) {
        if (c.pressed & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN)) { s_confirmYes = !s_confirmYes; audio_play_sfx(SFX_MOVE); }
        if (c.pressed & PAD_CIRCLE) { s_confirm = false; audio_play_sfx(SFX_BACK); }
        else if (c.pressed & PAD_CROSS) {
            s_confirm = false;
            if (s_confirmYes) {
                s_msgOk = leaderboard_reset();
                s_msgTimer = 2.5f;
                audio_play_sfx(SFX_CONFIRM);
            } else {
                audio_play_sfx(SFX_BACK);
            }
        }
        return SETTINGS_STAY;
    }
    if (c.pressed & PAD_UP)   { s_sel = (s_sel + ITEM_COUNT - 1) % ITEM_COUNT; audio_play_sfx(SFX_MOVE); }
    if (c.pressed & PAD_DOWN) { s_sel = (s_sel + 1) % ITEM_COUNT; audio_play_sfx(SFX_MOVE); }
    const SettingItem& it = ITEMS[s_sel];
    if (it.change && (c.pressed & PAD_LEFT))  { it.change(-1); audio_play_sfx(SFX_MOVE); }
    if (it.change && (c.pressed & PAD_RIGHT)) { it.change(1); audio_play_sfx(SFX_MOVE); }
    if (it.activate && (c.pressed & PAD_CROSS)) { audio_play_sfx(SFX_CONFIRM); it.activate(); }
    if (c.pressed & PAD_CIRCLE) { audio_play_sfx(SFX_BACK); return SETTINGS_BACK; }
    return SETTINGS_STAY;
}

void settings_render(Canvas& c, float time) {
    ui::background(c, time);
    ui::title(c, "IMPOSTAZIONI", 40);

    int x = 160, w = c.w - 320, rowH = 52, y0 = 150;
    ui::panel(c, x, y0 - 20, w, ITEM_COUNT * rowH + 40);
    for (int i = 0; i < ITEM_COUNT; i++) {
        int y = y0 + i * rowH;
        if (i == s_sel) ui::selection_bar(c, x + 10, y - 6, w - 20, rowH - 8, time);
        bool danger = ITEMS[i].activate == reset_activate;
        font_draw_shadow(c, x + 34, y + 8, ITEMS[i].label, danger ? ui::COL_DANGER : ui::COL_TEXT, 2);
        if (ITEMS[i].value) {
            char v[48];
            ITEMS[i].value(v, sizeof(v));
            font_draw_shadow(c, x + w - 34 - font_width(v, 2), y + 8, v, ui::COL_ACCENT, 2);
        }
    }
    font_draw_centered(c, c.w / 2, y0 + ITEM_COUNT * rowH + 50,
                       "Le impostazioni valgono solo per questa sessione.", ui::COL_DIM, 2);

    if (s_confirm) {
        ui::message_box(c, "Resetta classifica", "Sei sicuro? Questa azione", "non si può annullare.", ui::COL_DANGER);
        // Yes / No buttons inside the box
        int by = c.h / 2 + 30, bw = 140, gap = 60;
        int bxYes = c.w / 2 - gap / 2 - bw, bxNo = c.w / 2 + gap / 2;
        for (int k = 0; k < 2; k++) {
            bool yes = k == 0, sel = yes == s_confirmYes;
            int bx = yes ? bxYes : bxNo;
            draw_rect(c, bx, by, bw, 36, sel ? (yes ? ui::COL_DANGER : ui::COL_ACCENT) : rgb(50, 46, 40));
            font_draw_centered(c, bx + bw / 2, by + 10, yes ? "Sì" : "No", sel ? 0xFF000000 : ui::COL_TEXT, 2);
        }
        ui::Hint h[] = { { PAD_LEFT, "Scegli" }, { PAD_CROSS, "Conferma" }, { PAD_CIRCLE, "Annulla" } };
        ui::hints(c, h, 3);
        return;
    }
    if (s_msgTimer > 0) {
        if (s_msgOk) ui::message_box(c, "Fatto", "La classifica è stata azzerata.", nullptr, ui::COL_GOOD);
        else ui::message_box(c, "Errore", "Impossibile eliminare il file", "leaderboard.dat", ui::COL_DANGER);
        return;
    }
    ui::Hint h[] = { { PAD_UP, "Muovi" }, { PAD_LEFT, "Cambia" }, { PAD_CROSS, "Seleziona" }, { PAD_CIRCLE, "Indietro" } };
    ui::hints(c, h, 4);
}
