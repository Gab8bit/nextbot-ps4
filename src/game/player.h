// The player (first person): move forward/back and strafe with the left stick, turn
// with the right stick, wall collision, sprint with stamina.
#pragma once
#include "map.h"

constexpr float PLAYER_HALF = 0.30f;      // collision box half-size (tiles)
constexpr float PLAYER_WALK = 3.6f;       // tiles per second
constexpr float PLAYER_SPRINT = 5.8f;
constexpr float PLAYER_TURN = 2.8f;       // max turn speed (radians per second) at sensitivity 1

struct Player {
    float x, y;
    float angle;          // view direction (radians, 0 = east, map y grows downwards)
    float stamina;        // 0..1
    bool sprinting;
    bool exhausted;       // stamina ran out: sprint locked until it recovers a bit
    float regenDelay;
    float walkPhase;      // head bob
    float moveAmount;     // 0..1 how fast we moved this frame (for the bob)
};

void player_init(Player* p, float x, float y, float angle);
// forward, strafe: -1..1 (stick), turn: -1..1 (right stick, + = right), sprint: button held.
void player_update(Player* p, const Map& m, float forward, float strafe, float turn, bool sprint, float dt);
// Direction with the longest straight free run from a tile (a sensible starting view).
float player_open_direction(const Map& m, int tx, int ty);
