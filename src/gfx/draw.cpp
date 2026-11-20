#include "draw.h"

static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }

// Clip a rectangle against the canvas; returns false if nothing is left.
static bool clip(const Canvas& c, int& x, int& y, int& w, int& h) {
    int x1 = imin(x + w, c.w), y1 = imin(y + h, c.h);
    x = imax(x, 0); y = imax(y, 0);
    w = x1 - x; h = y1 - y;
    return w > 0 && h > 0;
}

void draw_clear(Canvas& c, uint32_t col) {
    int n = c.w * c.h;
    for (int i = 0; i < n; i++) c.px[i] = col;
}

void draw_rect(Canvas& c, int x, int y, int w, int h, uint32_t col) {
    if (!clip(c, x, y, w, h)) return;
    for (int j = 0; j < h; j++) {
        uint32_t* p = c.px + (y + j) * c.w + x;
        for (int i = 0; i < w; i++) p[i] = col;
    }
}

void draw_rect_a(Canvas& c, int x, int y, int w, int h, uint32_t col, int alpha) {
    if (alpha <= 0) return;
    if (alpha >= 255) { draw_rect(c, x, y, w, h, col); return; }
    if (!clip(c, x, y, w, h)) return;
    for (int j = 0; j < h; j++) {
        uint32_t* p = c.px + (y + j) * c.w + x;
        for (int i = 0; i < w; i++) p[i] = blend(p[i], col, (uint32_t)alpha);
    }
}

void draw_frame(Canvas& c, int x, int y, int w, int h, int t, uint32_t col) {
    draw_rect(c, x, y, w, t, col);
    draw_rect(c, x, y + h - t, w, t, col);
    draw_rect(c, x, y + t, t, h - 2 * t, col);
    draw_rect(c, x + w - t, y + t, t, h - 2 * t, col);
}

void draw_gradient_v(Canvas& c, int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    if (h <= 0) return;
    for (int j = 0; j < h; j++) {
        uint32_t a = (uint32_t)(j * 255 / (h > 1 ? h - 1 : 1));
        draw_rect(c, x, y + j, w, 1, blend(top, bottom, a));
    }
}

void draw_ellipse_a(Canvas& c, int cx, int cy, int rx, int ry, uint32_t col, int alpha) {
    if (rx <= 0 || ry <= 0) return;
    for (int j = -ry; j <= ry; j++) {
        int y = cy + j;
        if (y < 0 || y >= c.h) continue;
        // half width of the ellipse at this row
        float t = 1.0f - (float)(j * j) / (float)(ry * ry);
        if (t <= 0) continue;
        int hw = 0;
        while ((float)((hw + 1) * (hw + 1)) <= t * (float)(rx * rx)) hw++;
        draw_rect_a(c, cx - hw, y, hw * 2 + 1, 1, col, alpha);
    }
}

void draw_triangle(Canvas& c, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t col) {
    // sort by y
    if (y1 < y0) { float t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y2 < y0) { float t = y0; y0 = y2; y2 = t; t = x0; x0 = x2; x2 = t; }
    if (y2 < y1) { float t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
    int ys = (int)(y0 + 0.5f), ye = (int)(y2 + 0.5f);
    for (int y = ys; y < ye; y++) {
        float fy = (float)y + 0.5f;
        if (fy < y0 || fy > y2) continue;
        float xa = x0 + (x2 - x0) * (fy - y0) / (y2 - y0 + 1e-6f);
        float xb;
        if (fy < y1) xb = x0 + (x1 - x0) * (fy - y0) / (y1 - y0 + 1e-6f);
        else         xb = x1 + (x2 - x1) * (fy - y1) / (y2 - y1 + 1e-6f);
        if (xa > xb) { float t = xa; xa = xb; xb = t; }
        int ia = (int)(xa + 0.5f), ib = (int)(xb + 0.5f);
        draw_rect(c, ia, y, ib - ia, 1, col);
    }
}

void draw_apply_mask(Canvas& c, const uint8_t* mask) {
    int n = c.w * c.h;
    for (int i = 0; i < n; i++) {
        uint32_t m = mask[i];
        if (m == 255) continue;
        c.px[i] = shade(c.px[i], m);
    }
}
