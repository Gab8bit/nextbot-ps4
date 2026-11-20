// Tile maps: static definitions ('0' = free, '1' = wall) and collision helpers.
// World units are tiles: tile (x, y) covers [x, x+1) x [y, y+1).
#pragma once
#include <stdint.h>

constexpr int MAP_MAX_W = 64;
constexpr int MAP_MAX_H = 64;

enum MapTheme { THEME_BACKROOMS, THEME_CONCRETE };

struct MapDef {
    const char* name;
    MapTheme theme;
    int w, h;
    int spawnX, spawnY;      // player start tile
    const char* const* rows; // h strings of w characters
};

struct Map {
    const char* name;
    MapTheme theme;
    int w, h;
    int spawnX, spawnY;
    uint8_t tiles[MAP_MAX_W * MAP_MAX_H]; // 0 = free, 1 = wall
};

int map_count();
const MapDef* map_def(int index);
// Validates and loads a map. The outer border is forced to wall and free tiles that
// cannot be reached from the spawn are turned into walls (no unreachable pockets).
bool map_load(int index, Map* out);

static inline bool map_wall(const Map& m, int x, int y) {
    if (x < 0 || y < 0 || x >= m.w || y >= m.h) return true;
    return m.tiles[y * m.w + x] != 0;
}

// Does an axis-aligned box centred on (cx, cy) with half-size 'half' touch a wall?
bool map_box_hits_wall(const Map& m, float cx, float cy, float half);
// Moves a box by (dx, dy) sliding along walls (each axis resolved separately).
void map_move_box(const Map& m, float* x, float* y, float dx, float dy, float half);
// True if a box of half-size 'half' can travel in a straight line between the points.
bool map_line_clear(const Map& m, float x0, float y0, float x1, float y1, float half);
