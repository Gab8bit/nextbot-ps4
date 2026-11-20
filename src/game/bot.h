// The nextbot: chases the player along an A* path recomputed at fixed intervals,
// cutting corners whenever it has a clear line of movement. It speeds up over time.
#pragma once
#include "map.h"

constexpr int   BOT_PATH_MAX = 512;
constexpr float BOT_REPATH_INTERVAL = 0.25f; // seconds between A* searches
constexpr float BOT_HALF = 0.30f;            // clearance used for straight-line moves
constexpr float BOT_CATCH_RADIUS = 0.75f;    // game over below this distance (tiles)

enum Difficulty { DIFF_SLOW, DIFF_MEDIUM, DIFF_FAST, DIFF_COUNT };
const char* difficulty_name(int d);
float difficulty_base_speed(int d);

struct Bot {
    float x, y;
    float baseSpeed, speed;
    float elapsed;
    float repathTimer;
    int path[BOT_PATH_MAX];
    int pathLen, pathIdx;
    int repaths;        // number of A* searches done (diagnostics / tests)
};

void bot_init(Bot* b, float x, float y, float baseSpeed);
void bot_update(Bot* b, const Map& m, float px, float py, float dt);

// Spatial audio helpers: volume (never 0) and stereo pan from the bot-player offset.
float bot_volume_for_distance(float dist);
// side: where the bot is relative to where the player looks, -1 (left) .. 1 (right)
float bot_pan_for_side(float side);
