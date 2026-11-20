// Application state machine (platform independent, driven by main.cpp):
//   MENU -> SELECT -> PLAYING -> GAME_OVER -> SELECT/MENU
//   MENU -> LEADERBOARD -> MENU,  MENU -> SETTINGS -> MENU
#pragma once
#include "../gfx/draw.h"
#include "../platform/input.h"

enum AppState { ST_BOOT, ST_MENU, ST_SELECT, ST_LOADING, ST_PLAYING, ST_GAME_OVER, ST_LEADERBOARD, ST_SETTINGS };

void app_init(const char* userName);
// One frame: update with the pad state and dt (seconds), then draw into the canvas.
void app_frame(const PadRaw& pad, float dt, Canvas& canvas);
void app_shutdown();
AppState app_state();
