#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../gfx/font.h"

namespace ui {

void background(Canvas& c, float time) {
    draw_gradient_v(c, 0, 0, c.w, c.h, rgb(24, 20, 14), rgb(8, 7, 10));
    // faint drifting scan lines, VHS style
    for (int i = 0; i < 6; i++) {
        int y = (int)fmodf(time * (18.0f + i * 7.0f) + i * 97.0f, (float)c.h);
        draw_rect_a(c, 0, y, c.w, 2, rgb(255, 230, 160), 10);
    }
    // vignette-ish bands at the edges
    draw_rect_a(c, 0, 0, c.w, 6, 0xFF000000, 120);
    draw_rect_a(c, 0, c.h - 6, c.w, 6, 0xFF000000, 120);
}

void title(Canvas& c, const char* text, int y) {
    font_draw_centered(c, c.w / 2, y, text, COL_ACCENT, 4);
    int w = font_width(text, 4);
    draw_rect(c, c.w / 2 - w / 2, y + 40, w, 3, COL_ACCENT);
}

void panel(Canvas& c, int x, int y, int w, int h) {
    draw_rect_a(c, x, y, w, h, COL_PANEL, 215);
    draw_frame(c, x, y, w, h, 2, rgb(70, 62, 44));
}

void selection_bar(Canvas& c, int x, int y, int w, int h, float time) {
    int pulse = 70 + (int)(40 * sinf(time * 5.0f));
    draw_rect_a(c, x, y, w, h, COL_ACCENT, pulse);
    draw_rect(c, x, y, 4, h, COL_ACCENT);
}

void button_icon(Canvas& c, int x, int y, uint32_t b) {
    draw_ellipse_a(c, x + 9, y + 9, 9, 9, rgb(235, 235, 235), 255);
    draw_ellipse_a(c, x + 9, y + 9, 7, 7, rgb(30, 30, 36), 255);
    switch (b) {
        case PAD_CROSS: {
            uint32_t k = rgb(120, 170, 255);
            for (int i = -3; i <= 3; i++) {
                draw_rect(c, x + 9 + i - 1, y + 9 + i - 1, 2, 2, k);
                draw_rect(c, x + 9 + i - 1, y + 9 - i - 1, 2, 2, k);
            }
            break;
        }
        case PAD_CIRCLE:
            draw_ellipse_a(c, x + 9, y + 9, 4, 4, rgb(255, 100, 110), 255);
            draw_ellipse_a(c, x + 9, y + 9, 2, 2, rgb(30, 30, 36), 255);
            break;
        case PAD_TRIANGLE: {
            uint32_t k = rgb(90, 220, 170);
            draw_triangle(c, x + 9.0f, y + 4.0f, x + 4.0f, y + 13.0f, x + 14.0f, y + 13.0f, k);
            draw_triangle(c, x + 9.0f, y + 7.5f, x + 6.5f, y + 11.5f, x + 11.5f, y + 11.5f, rgb(30, 30, 36));
            break;
        }
        case PAD_SQUARE:
            draw_frame(c, x + 5, y + 5, 9, 9, 2, rgb(240, 140, 220));
            break;
        case PAD_UP: case PAD_DOWN: case PAD_LEFT: case PAD_RIGHT: {
            // d-pad: small cross, the relevant arms highlighted
            uint32_t on = rgb(235, 235, 235), off = rgb(90, 90, 96);
            bool vert = b == PAD_UP || b == PAD_DOWN;
            draw_rect(c, x + 7, y + 3, 4, 12, vert ? on : off);
            draw_rect(c, x + 3, y + 7, 12, 4, vert ? off : on);
            break;
        }
        default: {
            // shoulder/trigger/options: short text label
            const char* t = b == PAD_L1 ? "L1" : b == PAD_R1 ? "R1" : b == PAD_R2 ? "R2" : b == PAD_L2 ? "L2"
                          : b == PAD_OPTIONS ? "OP" : "?";
            font_draw(c, x + 9 - font_width(t, 1) / 2, y + 5, t, rgb(235, 235, 235), 1);
            break;
        }
    }
}

void hints(Canvas& c, const Hint* list, int count) {
    int total = 0;
    for (int i = 0; i < count; i++) total += 22 + font_width(list[i].label, 2) + 28;
    int x = c.w / 2 - total / 2;
    int y = c.h - 34;
    draw_rect_a(c, 0, y - 10, c.w, 44, 0xFF000000, 150);
    for (int i = 0; i < count; i++) {
        button_icon(c, x, y, list[i].button);
        font_draw_shadow(c, x + 24, y + 2, list[i].label, COL_TEXT, 2);
        x += 22 + font_width(list[i].label, 2) + 28;
    }
}

void ellipsize(char* dst, int dstLen, const char* src, int maxChars) {
    if (font_length(src) <= maxChars) { snprintf(dst, dstLen, "%s", src); return; }
    // copy maxChars-3 code points then "..."
    int keep = maxChars > 3 ? maxChars - 3 : 0, n = 0, o = 0;
    const unsigned char* p = (const unsigned char*)src;
    while (*p && n < keep) {
        int len = (*p < 0x80) ? 1 : (*p >= 0xF0) ? 4 : (*p >= 0xE0) ? 3 : (*p >= 0xC0) ? 2 : 1;
        if (o + len >= dstLen - 4) break;
        for (int i = 0; i < len && p[i]; i++) dst[o++] = (char)p[i];
        p += len; n++;
    }
    snprintf(dst + o, dstLen - o, "...");
}

void message_box(Canvas& c, const char* t, const char* l1, const char* l2, uint32_t titleColor) {
    draw_rect_a(c, 0, 0, c.w, c.h, 0xFF000000, 150);
    int w = 700, h = 210, x = (c.w - w) / 2, y = (c.h - h) / 2 - 20;
    draw_rect(c, x, y, w, h, COL_PANEL);
    draw_frame(c, x, y, w, h, 2, rgb(70, 62, 44));
    font_draw_centered(c, c.w / 2, y + 26, t, titleColor, 3);
    if (l1) font_draw_centered(c, c.w / 2, y + 84, l1, COL_TEXT, 2);
    if (l2) font_draw_centered(c, c.w / 2, y + 112, l2, COL_TEXT, 2);
}

} // namespace ui
