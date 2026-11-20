// Software rendering primitives on a 32-bit ARGB canvas.
// The game draws on a 960x540 canvas; the platform layer scales it 2x to 1920x1080.
#pragma once
#include <stdint.h>

constexpr int SCREEN_W = 960;
constexpr int SCREEN_H = 540;

struct Canvas {
    uint32_t* px;
    int w, h;
};

static inline uint32_t rgb(int r, int g, int b) {
    return 0xFF000000u | ((uint32_t)(r & 255) << 16) | ((uint32_t)(g & 255) << 8) | (uint32_t)(b & 255);
}

// Mix src over dst with coverage a (0..255). Alpha channel of the result is opaque.
static inline uint32_t blend(uint32_t d, uint32_t s, uint32_t a) {
    uint32_t na = 255 - a;
    uint32_t rb = (((s & 0xFF00FFu) * a + (d & 0xFF00FFu) * na) >> 8) & 0xFF00FFu;
    uint32_t g  = (((s & 0x00FF00u) * a + (d & 0x00FF00u) * na) >> 8) & 0x00FF00u;
    return 0xFF000000u | rb | g;
}

// Multiply the color by m/256 (m in 0..256).
static inline uint32_t shade(uint32_t c, uint32_t m) {
    uint32_t rb = (((c & 0xFF00FFu) * m) >> 8) & 0xFF00FFu;
    uint32_t g  = (((c & 0x00FF00u) * m) >> 8) & 0x00FF00u;
    return 0xFF000000u | rb | g;
}

void draw_clear(Canvas& c, uint32_t col);
void draw_rect(Canvas& c, int x, int y, int w, int h, uint32_t col);
void draw_rect_a(Canvas& c, int x, int y, int w, int h, uint32_t col, int alpha);
void draw_frame(Canvas& c, int x, int y, int w, int h, int t, uint32_t col);
void draw_gradient_v(Canvas& c, int x, int y, int w, int h, uint32_t top, uint32_t bottom);
void draw_ellipse_a(Canvas& c, int cx, int cy, int rx, int ry, uint32_t col, int alpha);
void draw_triangle(Canvas& c, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t col);
// Darken the whole canvas with a per-pixel 0..256 mask (same size as the canvas).
void draw_apply_mask(Canvas& c, const uint8_t* mask);
