#include "player.h"
#include <math.h>

static const float STAMINA_DRAIN = 0.30f;  // per second while sprinting (~3.3 s of sprint)
static const float STAMINA_REGEN = 0.22f;
static const float REGEN_DELAY = 0.7f;
static const float EXHAUST_RECOVER = 0.35f;
static const float PI = 3.14159265f;

void player_init(Player* p, float x, float y, float angle) {
    p->x = x; p->y = y;
    p->angle = angle;
    p->stamina = 1;
    p->sprinting = false;
    p->exhausted = false;
    p->regenDelay = 0;
    p->walkPhase = 0;
    p->moveAmount = 0;
}

void player_update(Player* p, const Map& m, float forward, float strafe, float turn, bool sprint, float dt) {
    p->angle += turn * PLAYER_TURN * dt;
    if (p->angle > PI) p->angle -= 2 * PI;
    if (p->angle < -PI) p->angle += 2 * PI;

    float mag = sqrtf(forward * forward + strafe * strafe);
    if (mag > 1.0f) { forward /= mag; strafe /= mag; mag = 1.0f; }
    bool moving = mag > 0.05f;

    p->sprinting = sprint && moving && forward > 0.1f && !p->exhausted && p->stamina > 0; // sprint only forwards
    if (p->sprinting) {
        p->stamina -= STAMINA_DRAIN * dt;
        p->regenDelay = REGEN_DELAY;
        if (p->stamina <= 0) { p->stamina = 0; p->exhausted = true; p->sprinting = false; }
    } else {
        if (p->regenDelay > 0) p->regenDelay -= dt;
        else p->stamina += STAMINA_REGEN * dt;
        if (p->stamina > 1) p->stamina = 1;
        if (p->exhausted && p->stamina >= EXHAUST_RECOVER) p->exhausted = false;
    }

    p->moveAmount = moving ? mag : 0;
    if (!moving) return;
    float speed = p->sprinting ? PLAYER_SPRINT : PLAYER_WALK;
    float dx = cosf(p->angle), dy = sinf(p->angle);
    float rx = -dy, ry = dx; // right-hand vector (map y grows downwards)
    float mx = dx * forward + rx * strafe, my = dy * forward + ry * strafe;
    map_move_box(m, &p->x, &p->y, mx * speed * dt, my * speed * dt, PLAYER_HALF);
    p->walkPhase += dt * speed * 2.4f * mag;
}

float player_open_direction(const Map& m, int tx, int ty) {
    const int dx[4] = { 1, 0, -1, 0 }, dy[4] = { 0, 1, 0, -1 };
    int best = 0, bestLen = -1;
    for (int k = 0; k < 4; k++) {
        int len = 0;
        while (!map_wall(m, tx + dx[k] * (len + 1), ty + dy[k] * (len + 1))) len++;
        if (len > bestLen) { bestLen = len; best = k; }
    }
    return best * PI / 2.0f;
}
