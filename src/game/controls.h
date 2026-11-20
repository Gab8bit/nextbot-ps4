// Turns raw pad state into "pressed this frame" edges, with auto-repeat for the
// directions (menus) and the left stick mapped to the D-pad in menus.
#pragma once
#include <stdint.h>
#include "../platform/input.h"

struct Controls {
    bool connected;
    uint32_t held;      // PAD_* bits currently down (stick folded into the directions)
    uint32_t pressed;   // went down this frame (directions also repeat while held)
    float lx, ly;       // left stick with a radial dead zone applied
    float rx, ry;       // right stick, same
    float r2, l2;
};

void controls_update(Controls* c, const PadRaw& raw, float dt);
