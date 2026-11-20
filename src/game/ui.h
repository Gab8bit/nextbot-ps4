// Shared UI drawing: backgrounds, panels, titles, list rows, button hints.
#pragma once
#include "../gfx/draw.h"
#include "../platform/input.h"

namespace ui {

constexpr uint32_t COL_TEXT   = 0xFFEDE6D6;
constexpr uint32_t COL_DIM    = 0xFF9A9384;
constexpr uint32_t COL_ACCENT = 0xFFFFD34D; // yellow
constexpr uint32_t COL_DANGER = 0xFFFF4A3D;
constexpr uint32_t COL_GOOD   = 0xFF6BE38A;
constexpr uint32_t COL_PANEL  = 0xFF15131A;

struct Hint { uint32_t button; const char* label; };

void background(Canvas& c, float time);
void title(Canvas& c, const char* text, int y);
void panel(Canvas& c, int x, int y, int w, int h);
void selection_bar(Canvas& c, int x, int y, int w, int h, float time);
// Small PlayStation-style button icon (PAD_CROSS, PAD_CIRCLE, ...) at (x, y), 18x18.
void button_icon(Canvas& c, int x, int y, uint32_t button);
// Row of hints at the bottom of the screen.
void hints(Canvas& c, const Hint* list, int count);
// Fits text into maxChars glyphs, adding "..." when truncated.
void ellipsize(char* dst, int dstLen, const char* src, int maxChars);
// Centred message box (used for errors and confirmations).
void message_box(Canvas& c, const char* title, const char* line1, const char* line2, uint32_t titleColor);

} // namespace ui
