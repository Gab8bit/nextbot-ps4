// Settings screen: a list of items (easy to extend) and the "reset leaderboard"
// confirmation. Settings other than the leaderboard are NOT saved (session only).
#pragma once
#include "controls.h"
#include "../gfx/draw.h"

struct AppSettings {
    float volume;        // master volume 0..1
    int difficulty;      // default difficulty for new games (Difficulty)
    float lookSpeed;     // right-stick turn sensitivity 0.5..2
};
extern AppSettings g_settings;

enum SettingsResult { SETTINGS_STAY, SETTINGS_BACK };

void settings_open();
SettingsResult settings_update(const Controls& c, float dt);
void settings_render(Canvas& c, float time);
