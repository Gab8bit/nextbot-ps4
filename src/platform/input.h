// DualShock 4 input via libScePad, mapped to platform-independent button bits.
#pragma once
#include <stdint.h>

enum : uint32_t {
    PAD_UP       = 1u << 0,
    PAD_DOWN     = 1u << 1,
    PAD_LEFT     = 1u << 2,
    PAD_RIGHT    = 1u << 3,
    PAD_CROSS    = 1u << 4,
    PAD_CIRCLE   = 1u << 5,
    PAD_SQUARE   = 1u << 6,
    PAD_TRIANGLE = 1u << 7,
    PAD_L1       = 1u << 8,
    PAD_R1       = 1u << 9,
    PAD_L2       = 1u << 10,
    PAD_R2       = 1u << 11,
    PAD_OPTIONS  = 1u << 12,
};

struct PadRaw {
    bool connected;
    uint32_t buttons;  // PAD_* bits
    float lx, ly;      // left stick, -1..1 (y down = positive), no dead zone applied
    float rx, ry;      // right stick, same convention
    float l2, r2;      // analog triggers 0..1
};

bool input_init(int userId);
void input_read(PadRaw* out);
// Lightbar color (0xRRGGBB) and vibration (0..255). Only sent to the pad when it changes.
void input_set_feedback(uint32_t lightRgb, uint8_t rumbleBig, uint8_t rumbleSmall);
void input_shutdown();
