#include "bot.h"
#include "pathfind.h"
#include <math.h>

static const float ACCEL = 0.018f;      // tiles/s gained every second
static const float MAX_BONUS = 2.6f;    // cap on the speed gained over time
static const int   LOOKAHEAD = 8;       // path nodes checked for a straight shortcut

static const char* DIFF_NAMES[DIFF_COUNT] = { "Lenta", "Media", "Veloce" };
static const float DIFF_SPEED[DIFF_COUNT] = { 2.7f, 3.4f, 4.2f };

const char* difficulty_name(int d) { return (d >= 0 && d < DIFF_COUNT) ? DIFF_NAMES[d] : "?"; }
float difficulty_base_speed(int d) { return (d >= 0 && d < DIFF_COUNT) ? DIFF_SPEED[d] : DIFF_SPEED[1]; }

void bot_init(Bot* b, float x, float y, float baseSpeed) {
    b->x = x; b->y = y;
    b->baseSpeed = baseSpeed;
    b->speed = baseSpeed;
    b->elapsed = 0;
    b->repathTimer = 0;
    b->pathLen = 0;
    b->pathIdx = 0;
    b->repaths = 0;
}

void bot_update(Bot* b, const Map& m, float px, float py, float dt) {
    b->elapsed += dt;
    float bonus = ACCEL * b->elapsed;
    b->speed = b->baseSpeed + (bonus < MAX_BONUS ? bonus : MAX_BONUS);

    b->repathTimer -= dt;
    if (b->repathTimer <= 0) {
        b->repathTimer = BOT_REPATH_INTERVAL;
        b->pathLen = pathfind_astar(m, (int)b->x, (int)b->y, (int)px, (int)py, b->path, BOT_PATH_MAX);
        b->pathIdx = b->pathLen > 1 ? 1 : 0; // node 0 is the tile the bot is standing on
        b->repaths++;
    }

    // Pick where to head this frame.
    float tx, ty;
    bool onPath = false;
    if (map_line_clear(m, b->x, b->y, px, py, BOT_HALF)) {
        tx = px; ty = py;                         // straight at the player
    } else if (b->pathLen > 0) {
        int last = b->pathIdx + LOOKAHEAD;
        if (last > b->pathLen - 1) last = b->pathLen - 1;
        int pick = -1;
        for (int k = last; k >= b->pathIdx; k--) { // farthest visible node = smooth corner cutting
            float nx = (b->path[k] % m.w) + 0.5f, ny = (b->path[k] / m.w) + 0.5f;
            if (map_line_clear(m, b->x, b->y, nx, ny, BOT_HALF)) { pick = k; break; }
        }
        if (pick >= 0) {
            b->pathIdx = pick;
            tx = (b->path[pick] % m.w) + 0.5f; ty = (b->path[pick] / m.w) + 0.5f;
            onPath = true;
        } else {
            // re-centre in the current tile; from there the next node is always reachable
            tx = floorf(b->x) + 0.5f; ty = floorf(b->y) + 0.5f;
        }
    } else {
        return; // no path (should not happen on a connected map)
    }

    float dx = tx - b->x, dy = ty - b->y;
    float d = sqrtf(dx * dx + dy * dy);
    float step = b->speed * dt;
    if (d <= step) {
        b->x = tx; b->y = ty;
        if (onPath && b->pathIdx < b->pathLen - 1) b->pathIdx++;
    } else {
        b->x += dx / d * step;
        b->y += dy / d * step;
    }
}

float bot_volume_for_distance(float dist) {
    const float MIN_VOL = 0.07f, RANGE = 18.0f;
    float t = 1.0f - dist / RANGE;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return MIN_VOL + (1.0f - MIN_VOL) * t * t;
}

float bot_pan_for_side(float side) {
    if (side < -1) side = -1;
    if (side > 1) side = 1;
    return side * 0.7f;
}
