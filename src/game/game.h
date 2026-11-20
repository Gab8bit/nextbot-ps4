// A play session: map + player + bot, HUD, pause, and the game over screen.
#pragma once
#include <stdint.h>
#include "controls.h"
#include "leaderboard.h"
#include "../gfx/draw.h"

// Loads the bot sprite/sound and the map. On failure returns false with a message.
bool game_start(int botIndex, int difficulty, int mapIndex, char* err, int errLen);
void game_restart();   // same bot and map, fresh run (assets are kept)
void game_end();       // stops the sound and frees the session assets
void game_shutdown();  // game_end + frees the buffers kept between sessions

enum GameEvent { GAME_RUNNING, GAME_CAUGHT, GAME_QUIT };
GameEvent game_update(const Controls& c, float dt);
void game_render(Canvas& c, float time);

uint32_t game_survival_ms();
const char* game_bot_name();

// ---- game over ----
enum GameOverResult { GO_STAY, GO_MENU, GO_RETRY };
// prevBest: the record for this bot before this run (nullptr if there was none).
void gameover_open(bool newRecord, const LbRecord* prevBest);
GameOverResult gameover_update(const Controls& c, float dt);
void gameover_render(Canvas& c, float time);

// ---- test hooks (used by the PC harness) ----
void game_debug_set_bot(float x, float y);
float game_debug_distance();
void game_debug_positions(float* px, float* py, float* bx, float* by);
float game_debug_angle();
void game_debug_set_angle(float a);
