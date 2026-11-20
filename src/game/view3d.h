// First-person view: grid raycaster (Wolfenstein-style 2.5D) with textured walls,
// floor and ceiling, distance fog, flickering ceiling lights and the bot drawn as a
// billboard hidden by walls (z-buffer). Renders at 480x270 and scales 2x to the canvas.
#pragma once
#include "map.h"
#include "../gfx/draw.h"
#include "../gfx/sprite.h"

constexpr int VIEW_W = 480;
constexpr int VIEW_H = 270;

struct Camera {
    float x, y;      // position (tiles)
    float angle;     // radians, 0 = +x (east), y grows "down" the map
    float bob;       // vertical head bob in view pixels
};

struct Billboard {
    const Sprite* sprite; // texture (resized once at load time)
    float x, y;           // position (tiles)
    float height;         // world height (1 = wall height)
    float lift;           // hovering above the floor (world units)
};

// Builds the textures for a theme (call once per game start).
bool view3d_init(MapTheme theme);
void view3d_free();
// light: 0..1 global brightness (used for the flickering neon).
void view3d_render(Canvas& out, const Map& m, const Camera& cam, const Billboard* bot, float light);
// True if the bot's centre was visible (not hidden by walls) in the last render.
bool view3d_bot_visible();
