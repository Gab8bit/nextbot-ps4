#include "controls.h"
#include <math.h>

static const uint32_t DIRS = PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT;
static const float REPEAT_DELAY = 0.35f, REPEAT_RATE = 0.085f;
static const float DEAD_ZONE = 0.22f;

static uint32_t s_prev = 0;
static float s_repeatTimer = 0;

// Radial dead zone, rescaled so movement starts smoothly at the edge of the zone.
static void dead_zone(float x, float y, float* ox, float* oy) {
    float mag = sqrtf(x * x + y * y);
    if (mag < DEAD_ZONE) { *ox = *oy = 0; return; }
    float m = (mag - DEAD_ZONE) / (1.0f - DEAD_ZONE);
    if (m > 1.0f) m = 1.0f;
    *ox = x / mag * m;
    *oy = y / mag * m;
}

void controls_update(Controls* c, const PadRaw& raw, float dt) {
    c->connected = raw.connected;
    c->l2 = raw.l2;
    c->r2 = raw.r2;

    dead_zone(raw.lx, raw.ly, &c->lx, &c->ly);
    dead_zone(raw.rx, raw.ry, &c->rx, &c->ry);

    uint32_t held = raw.buttons;
    if (raw.ly < -0.6f) held |= PAD_UP;
    if (raw.ly > 0.6f) held |= PAD_DOWN;
    if (raw.lx < -0.6f) held |= PAD_LEFT;
    if (raw.lx > 0.6f) held |= PAD_RIGHT;

    uint32_t pressed = held & ~s_prev;
    if (held & DIRS) {
        if (pressed & DIRS) s_repeatTimer = REPEAT_DELAY;
        else {
            s_repeatTimer -= dt;
            if (s_repeatTimer <= 0) {
                pressed |= held & DIRS;
                s_repeatTimer += REPEAT_RATE;
            }
        }
    }
    c->held = held;
    c->pressed = pressed;
    s_prev = held;
}
