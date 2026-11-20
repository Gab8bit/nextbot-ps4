// 8x8 bitmap font (ASCII 32..126 plus Italian accented vowels), monospaced.
// Strings are UTF-8; unsupported characters are drawn as '?'.
#pragma once
#include <stdint.h>
#include "draw.h"

constexpr int FONT_CELL = 8;

void font_draw(Canvas& c, int x, int y, const char* text, uint32_t color, int scale);
// Draws with a 1-scale-pixel dark drop shadow, which keeps text readable on any background.
void font_draw_shadow(Canvas& c, int x, int y, const char* text, uint32_t color, int scale);
void font_draw_centered(Canvas& c, int cx, int y, const char* text, uint32_t color, int scale);
int  font_width(const char* text, int scale);
int  font_length(const char* text); // number of glyphs
