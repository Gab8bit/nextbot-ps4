#include "map.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Map 1: "Backrooms" - open rooms with pillars and partitions, plenty of loops.
static const char* const MAP_BACKROOMS[] = {
    "1111111111111111111111111111111111111111",
    "1000000000000100000000000000100000000001",
    "1011001100000100110011001100100110011001",
    "1011001100000000110011001100000110011001",
    "1000000000000100000000000000100000000001",
    "1000000111111100000011111111100000110001",
    "1001100100000000000010000000000000110001",
    "1001100100110011000010001100110000000001",
    "1000000100110011000000001100110011111001",
    "1000000000000000000010000000000010000001",
    "1111110001111111100011111100011110001111",
    "1000000001000000000000000100000000001001",
    "1001100001001100110011000100110011001001",
    "1001100000001100110011000000110011000001",
    "1000000001000000000000000100000000001001",
    "1000111111110001111111100111111000111001",
    "1000100000000001000000000000001000001001",
    "1110100110011001001100110011001001101001",
    "1000000110011000001100110011000001100001",
    "1000100000000001000000000000001000000001",
    "1011111100011111110001111111111100011101",
    "1000000000010000000000000010000000000001",
    "1001100110010011001100110010011001100001",
    "1001100110000011001100110000011001100001",
    "1000000000010000000000000010000000000001",
    "1111111111111111111111111111111111111111",
};

// Map 2: "Bunker" - concrete corridors, maze-like but with several loops.
static const char* const MAP_BUNKER[] = {
    "1111111111111111111111111111111111",
    "1000001000000000100000000010000001",
    "1011101011111110101111111010111101",
    "1010001000000010100000001000100001",
    "1010111111101010111110101111101111",
    "1000100000101000000010100000001001",
    "1110101110101111111010111111101011",
    "1000001000100000001010000000100001",
    "1011111011111011101011111110111101",
    "1000000010000010001000000010000001",
    "1011111110111110111110111011111101",
    "1000000000100000100000100000000001",
    "1111011110101110101110101111011111",
    "1000010000101000100010100001000001",
    "1011010111101011111010111101011101",
    "1001010100001000000010000101010001",
    "1101010101111111011111110101010111",
    "1000000101000000010000000101000001",
    "1011111101011111010111110101111101",
    "1000000001000001000100000100000001",
    "1011111011111101110111011111011101",
    "1000001000000000000000000000010001",
    "1000000000100000100000100000000001",
    "1111111111111111111111111111111111",
};

static const MapDef MAPS[] = {
    { "Backrooms", THEME_BACKROOMS, 40, 26, 2, 24, MAP_BACKROOMS },
    { "Bunker",    THEME_CONCRETE,  34, 24, 1, 22, MAP_BUNKER },
};

int map_count() { return (int)(sizeof(MAPS) / sizeof(MAPS[0])); }
const MapDef* map_def(int i) { return (i >= 0 && i < map_count()) ? &MAPS[i] : nullptr; }

bool map_load(int index, Map* out) {
    const MapDef* d = map_def(index);
    if (!d || d->w < 3 || d->h < 3 || d->w > MAP_MAX_W || d->h > MAP_MAX_H) return false;
    memset(out, 0, sizeof(*out));
    out->name = d->name; out->theme = d->theme;
    out->w = d->w; out->h = d->h;
    out->spawnX = d->spawnX; out->spawnY = d->spawnY;
    for (int y = 0; y < d->h; y++) {
        const char* row = d->rows[y];
        if ((int)strlen(row) != d->w) { printf("[nextbot] map %s: row %d has wrong length\n", d->name, y); return false; }
        for (int x = 0; x < d->w; x++) {
            bool border = x == 0 || y == 0 || x == d->w - 1 || y == d->h - 1;
            out->tiles[y * d->w + x] = (row[x] != '0' || border) ? 1 : 0;
        }
    }
    if (map_wall(*out, out->spawnX, out->spawnY)) return false;

    // flood fill from the spawn; unreachable free tiles become walls
    static uint8_t seen[MAP_MAX_W * MAP_MAX_H];
    static int queue[MAP_MAX_W * MAP_MAX_H];
    memset(seen, 0, sizeof(seen));
    int head = 0, tail = 0;
    int start = out->spawnY * out->w + out->spawnX;
    queue[tail++] = start; seen[start] = 1;
    while (head < tail) {
        int c = queue[head++], cx = c % out->w, cy = c / out->w;
        const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        for (int k = 0; k < 4; k++) {
            int nx = cx + dx[k], ny = cy + dy[k];
            if (map_wall(*out, nx, ny)) continue;
            int n = ny * out->w + nx;
            if (!seen[n]) { seen[n] = 1; queue[tail++] = n; }
        }
    }
    for (int i = 0; i < out->w * out->h; i++)
        if (!out->tiles[i] && !seen[i]) out->tiles[i] = 1;
    return true;
}

bool map_box_hits_wall(const Map& m, float cx, float cy, float half) {
    const float eps = 1e-4f;
    int x0 = (int)floorf(cx - half), x1 = (int)floorf(cx + half - eps);
    int y0 = (int)floorf(cy - half), y1 = (int)floorf(cy + half - eps);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (map_wall(m, x, y)) return true;
    return false;
}

void map_move_box(const Map& m, float* px, float* py, float dx, float dy, float half) {
    const float eps = 1e-3f;
    // sub-steps keep every step well below one tile, so we never tunnel through a wall
    float len = fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy);
    int steps = (int)(len / 0.2f) + 1;
    float sx = dx / steps, sy = dy / steps;
    float x = *px, y = *py;
    for (int i = 0; i < steps; i++) {
        if (sx != 0) {
            float nx = x + sx;
            if (map_box_hits_wall(m, nx, y, half)) {
                // snap against the wall we hit
                nx = sx > 0 ? floorf(nx + half) - half - eps : floorf(nx - half) + 1.0f + half + eps;
                if (map_box_hits_wall(m, nx, y, half)) nx = x;
            }
            x = nx;
        }
        if (sy != 0) {
            float ny = y + sy;
            if (map_box_hits_wall(m, x, ny, half)) {
                ny = sy > 0 ? floorf(ny + half) - half - eps : floorf(ny - half) + 1.0f + half + eps;
                if (map_box_hits_wall(m, x, ny, half)) ny = y;
            }
            y = ny;
        }
    }
    *px = x; *py = y;
}

bool map_line_clear(const Map& m, float x0, float y0, float x1, float y1, float half) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    int steps = (int)(len / 0.15f) + 1;
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        if (map_box_hits_wall(m, x0 + dx * t, y0 + dy * t, half)) return false;
    }
    return true;
}
