// Menu screens: main menu, bot selection, leaderboard view.
#pragma once
#include "controls.h"
#include "../gfx/draw.h"

// ---- main menu: Gioca / Classifica / Impostazioni ----
enum MainChoice { MAIN_NONE, MAIN_PLAY, MAIN_LEADERBOARD, MAIN_SETTINGS };
void mainmenu_open(bool resetCursor);
MainChoice mainmenu_update(const Controls& c);
void mainmenu_render(Canvas& c, float time, const char* userName);

// ---- bot selection ----
enum SelectResult { SELECT_STAY, SELECT_BACK, SELECT_START, SELECT_RESCAN };
void botselect_open(bool resetCursor);
SelectResult botselect_update(const Controls& c);
void botselect_render(Canvas& c, float time);
int botselect_bot();
int botselect_difficulty();
int botselect_map();
// Shows an error popup (e.g. the PNG could not be loaded when starting).
void botselect_set_error(const char* msg);

// ---- leaderboard screen ----
void lbscreen_open();
bool lbscreen_update(const Controls& c); // true = go back
void lbscreen_render(Canvas& c, float time);
