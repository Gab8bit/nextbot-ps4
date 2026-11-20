#include "app.h"

#include <stdio.h>
#include <string.h>
#include "catalog.h"
#include "controls.h"
#include "game.h"
#include "leaderboard.h"
#include "menu.h"
#include "settings.h"
#include "ui.h"
#include "../gfx/font.h"
#include "../platform/audio.h"
#include "../platform/fs.h"

static AppState s_state = ST_BOOT;
static Controls s_ctl;
static char s_user[LB_USER_MAX];
static float s_time = 0;
static int s_bootFrames = 0;
static bool s_rescanPending = false;

AppState app_state() { return s_state; }

void app_init(const char* userName) {
    leaderboard_sanitize(s_user, sizeof(s_user), (userName && userName[0]) ? userName : "Player");
    memset(&s_ctl, 0, sizeof(s_ctl));
    audio_set_master(g_settings.volume);
    s_state = ST_BOOT;
    s_bootFrames = 0;
    mainmenu_open(true);
}

static void loading_screen(Canvas& c, const char* what) {
    ui::background(c, s_time);
    font_draw_centered(c, c.w / 2, c.h / 2 - 30, "Caricamento...", ui::COL_ACCENT, 3);
    if (what) font_draw_centered(c, c.w / 2, c.h / 2 + 20, what, ui::COL_DIM, 2);
}

static void go(AppState st) {
    s_state = st;
    switch (st) {
        case ST_MENU: mainmenu_open(false); input_set_feedback(0x100820, 0, 0); break;
        case ST_LEADERBOARD: lbscreen_open(); break;
        case ST_SETTINGS: settings_open(); break;
        default: break;
    }
}

void app_frame(const PadRaw& pad, float dt, Canvas& c) {
    if (dt > 0.1f) dt = 0.1f; // e.g. after a long load: never simulate a huge step
    s_time += dt;
    controls_update(&s_ctl, pad, dt);

    switch (s_state) {
    case ST_BOOT:
        // first frame: show the loading screen; second frame: do the slow work
        if (s_bootFrames++ == 0) { loading_screen(c, "Ricerca dei bot"); return; }
        catalog_ensure_folders();
        leaderboard_load();
        catalog_scan();
        printf("[nextbot] %d bot(s) found, %d record(s)\n", catalog_count(), leaderboard_count());
        go(ST_MENU);
        break;

    case ST_MENU:
        switch (mainmenu_update(s_ctl)) {
            case MAIN_PLAY: botselect_open(true); go(ST_SELECT); break;
            case MAIN_LEADERBOARD: go(ST_LEADERBOARD); break;
            case MAIN_SETTINGS: go(ST_SETTINGS); break;
            default: break;
        }
        break;

    case ST_SELECT:
        if (s_rescanPending) {
            s_rescanPending = false;
            catalog_scan();
            botselect_open(false);
        }
        switch (botselect_update(s_ctl)) {
            case SELECT_BACK: go(ST_MENU); break;
            case SELECT_START: s_state = ST_LOADING; s_bootFrames = 0; break;
            case SELECT_RESCAN:
                s_rescanPending = true;
                loading_screen(c, "Ricerca dei bot");
                return;
            default: break;
        }
        break;

    case ST_LOADING: {
        if (s_bootFrames++ == 0) {
            const BotEntry* b = catalog_get(botselect_bot());
            loading_screen(c, b ? b->name : nullptr);
            return;
        }
        char err[160];
        if (game_start(botselect_bot(), botselect_difficulty(), botselect_map(), err, sizeof(err))) {
            s_state = ST_PLAYING;
        } else {
            botselect_set_error(err);
            s_state = ST_SELECT;
        }
        break;
    }

    case ST_PLAYING:
        switch (game_update(s_ctl, dt)) {
            case GAME_CAUGHT: {
                uint32_t ms = game_survival_ms();
                const LbRecord* prev = leaderboard_find(game_bot_name());
                LbRecord prevCopy;
                if (prev) prevCopy = *prev;
                bool newRecord = leaderboard_submit(s_user, game_bot_name(), ms);
                printf("[nextbot] caught after %u ms%s\n", (unsigned)ms, newRecord ? " (new record)" : "");
                gameover_open(newRecord, prev ? &prevCopy : nullptr);
                s_state = ST_GAME_OVER;
                break;
            }
            case GAME_QUIT: game_end(); botselect_open(false); go(ST_SELECT); break;
            default: break;
        }
        break;

    case ST_GAME_OVER:
        switch (gameover_update(s_ctl, dt)) {
            case GO_MENU: game_end(); botselect_open(false); go(ST_SELECT); break;
            case GO_RETRY: game_restart(); s_state = ST_PLAYING; break;
            default: break;
        }
        break;

    case ST_LEADERBOARD:
        if (lbscreen_update(s_ctl)) go(ST_MENU);
        break;

    case ST_SETTINGS:
        if (settings_update(s_ctl, dt) == SETTINGS_BACK) go(ST_MENU);
        break;
    }

    // ---- render the (possibly new) state ----
    switch (s_state) {
        case ST_BOOT: case ST_LOADING: loading_screen(c, nullptr); break;
        case ST_MENU: mainmenu_render(c, s_time, s_user); break;
        case ST_SELECT: botselect_render(c, s_time); break;
        case ST_PLAYING: game_render(c, s_time); break;
        case ST_GAME_OVER: gameover_render(c, s_time); break;
        case ST_LEADERBOARD: lbscreen_render(c, s_time); break;
        case ST_SETTINGS: settings_render(c, s_time); break;
    }
    if (!s_ctl.connected) {
        draw_rect_a(c, 0, c.h / 2 - 30, c.w, 60, 0xFF000000, 200);
        font_draw_centered(c, c.w / 2, c.h / 2 - 8, "Collega il controller", ui::COL_ACCENT, 2);
    }
}

void app_shutdown() {
    game_shutdown();
    catalog_free();
}
