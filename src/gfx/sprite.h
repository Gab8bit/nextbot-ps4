// Sprites: PNG loading (stb_image), one-time resize, alpha blit.
#pragma once
#include <stdint.h>
#include "draw.h"

struct Sprite {
    int w = 0, h = 0;
    uint32_t* px = nullptr; // ARGB, straight (non-premultiplied) alpha
};

bool sprite_create(Sprite* s, int w, int h);
void sprite_free(Sprite* s);

// Decodes a PNG at full resolution. Rejects images larger than 4096x4096.
bool sprite_load_png(const char* path, Sprite* out, char* err, int errLen);

// Resizes src to fit inside boxW x boxH keeping the aspect ratio (box filter when
// shrinking, bilinear when enlarging). Call once at load time, never per frame.
bool sprite_resize_fit(const Sprite& src, int boxW, int boxH, Sprite* out);

// Draws with per-pixel alpha, multiplied by 'alpha' (0..255).
void sprite_blit(Canvas& c, const Sprite& s, int x, int y, int alpha = 255);
// Same, but every opaque pixel is mixed towards 'tint' by tintAmount (0..255).
void sprite_blit_tint(Canvas& c, const Sprite& s, int x, int y, uint32_t tint, int tintAmount);
