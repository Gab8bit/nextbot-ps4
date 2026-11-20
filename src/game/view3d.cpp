#include "view3d.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const int TEX = 64;                 // texture size (power of two)
static const float PLANE = 0.72f;          // camera plane half-width -> ~72 deg horizontal FOV
static const float K = VIEW_W / (2.0f * PLANE); // projection scale (keeps texels square)
static const int FOG_STEPS = 512;          // fog table: 1/16 tile resolution up to 32 tiles

struct Theme {
    uint32_t fog;          // colour things fade into
    float density;         // fog density per tile
    int lightEvery, lightOffset; // about one ceiling cell in 'lightEvery' has a neon panel
};

static uint32_t s_wallTex[TEX * TEX], s_wallDark[TEX * TEX];
static uint32_t s_floorTex[TEX * TEX], s_floorLit[TEX * TEX];
static uint32_t s_ceilTex[TEX * TEX], s_lightTex[TEX * TEX];
static uint8_t s_fog[FOG_STEPS];           // 0 = no fog, 255 = only fog
static Theme s_theme;
static uint32_t s_view[VIEW_W * VIEW_H];
static float s_zbuf[VIEW_W];
static bool s_botVisible = false;

// ---------------------------------------------------------------- textures ---
static uint32_t hash2(int x, int y, int seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Smooth value noise that tiles over the 64-pixel texture (cell = 64 / cells pixels).
static float vnoise(int x, int y, int cells, int seed) {
    int cs = TEX / cells;
    int gx = x / cs, gy = y / cs;
    float fx = (float)(x % cs) / cs, fy = (float)(y % cs) / cs;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    auto v = [&](int a, int b) { return (hash2(a % cells, b % cells, seed) & 1023) / 1023.0f; };
    float top = v(gx, gy) + (v(gx + 1, gy) - v(gx, gy)) * fx;
    float bot = v(gx, gy + 1) + (v(gx + 1, gy + 1) - v(gx, gy + 1)) * fx;
    return top + (bot - top) * fy;
}

static int clampi(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static uint32_t noisy(int r, int g, int b, int amount, uint32_t h) {
    int d = (int)(h % (uint32_t)(amount * 2 + 1)) - amount;
    return rgb(clampi(r + d), clampi(g + d), clampi(b + d));
}

static void build_backrooms() {
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) {
            uint32_t h = hash2(x, y, 11), i = y * TEX + x;
            // yellowed wallpaper: thin vertical lines, a faint chevron, soft damp stains, skirting board
            int line = (x % 8 == 0) ? -12 : 0;
            int cx8 = x % 8, cy8 = (y % 8);
            int chevron = (cx8 == 4 + (cy8 < 4 ? cy8 : 7 - cy8) / 2 || cx8 == 4 - (cy8 < 4 ? cy8 : 7 - cy8) / 2) ? -5 : 0;
            int stain = (int)(vnoise(x, y, 4, 5) * vnoise(x, y, 8, 6) * -34.0f);
            int grime = y < 3 ? -18 + y * 6 : 0;                      // darker line under the ceiling
            int k = line + chevron + stain + grime;
            uint32_t w = noisy(208 + k, 192 + k, 120 + k * 3 / 4, 3, h);
            if (y >= TEX - 6) w = noisy(118, 98, 56, 4, h);           // skirting board
            if (y == TEX - 7) w = rgb(88, 72, 38);
            s_wallTex[i] = w;
            // damp mustard carpet
            int weave = ((x / 2 + y / 2) & 1) ? 5 : -3;
            int damp = (int)(vnoise(x, y, 4, 9) * -26.0f);
            s_floorTex[i] = noisy(150 + weave + damp, 128 + weave + damp, 70 + weave + damp / 2, 8, h);
            // acoustic ceiling tiles: off-white with a grid
            uint32_t c = noisy(198, 192, 160, 6, h);
            if (x == 0 || y == 0 || x == 32 || y == 32) c = rgb(150, 144, 112);
            s_ceilTex[i] = c;
            // fluorescent panel
            bool frame = x < 6 || y < 6 || x >= TEX - 6 || y >= TEX - 6;
            s_lightTex[i] = frame ? rgb(170, 166, 140) : ((x / 6) % 2 ? rgb(255, 253, 232) : rgb(246, 244, 220));
        }
    s_theme = { rgb(40, 35, 15), 0.16f, 5, 0 };
}

static void build_concrete() {
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) {
            uint32_t h = hash2(x, y, 23), i = y * TEX + x;
            // concrete blocks
            uint32_t w = noisy(112, 112, 116, 8, h);
            bool mortar = (y % 16 == 0) || ((x + ((y / 16) % 2) * 16) % 32 == 0);
            if (mortar) w = rgb(78, 78, 82);
            if (hash2(x / 6, y / 6, 3) % 13 == 0) w = rgb(92, 90, 86); // damp patches
            s_wallTex[i] = w;
            uint32_t f = noisy(66, 66, 70, 10, h);
            if (x % 32 == 0 || y % 32 == 0) f = rgb(44, 44, 48);
            s_floorTex[i] = f;
            s_ceilTex[i] = noisy(52, 52, 56, 6, h);
            bool frame = x < 20 || x >= TEX - 20 || y < 4 || y >= TEX - 4;
            s_lightTex[i] = frame ? s_ceilTex[i] : rgb(225, 235, 255);
        }
    s_theme = { rgb(6, 6, 8), 0.2f, 7, 0 };
}

bool view3d_init(MapTheme theme) {
    if (theme == THEME_CONCRETE) build_concrete();
    else build_backrooms();
    for (int i = 0; i < TEX * TEX; i++) {
        s_wallDark[i] = shade(s_wallTex[i], 200);   // walls facing north/south are darker
        s_floorLit[i] = blend(s_floorTex[i], 0xFFFFFFF0u, 40); // floor under a light
    }
    for (int i = 0; i < FOG_STEPS; i++) {
        float d = i / 16.0f;
        float vis = expf(-d * s_theme.density);  // exponential fog
        if (d < 1.0f) vis = 1.0f;
        s_fog[i] = (uint8_t)(255.0f * (1.0f - vis));
    }
    return true;
}

void view3d_free() {}
bool view3d_bot_visible() { return s_botVisible; }

static inline uint32_t fog_of(uint32_t c, float dist, float light) {
    int idx = (int)(dist * 16.0f);
    if (idx >= FOG_STEPS) idx = FOG_STEPS - 1;
    if (idx < 0) idx = 0;
    uint32_t f = s_fog[idx];
    if (light < 0.999f) c = shade(c, (uint32_t)(light * 256.0f));
    return f ? blend(c, s_theme.fog, f) : c;
}

static bool is_light_cell(int cx, int cy) {
    // scattered pseudo-randomly, so every corridor gets some lights
    if (cx < 0 || cy < 0) return false;
    return (int)((unsigned)(cx * 7 + cy * 13) % (unsigned)s_theme.lightEvery) == s_theme.lightOffset;
}

// ------------------------------------------------------------------ render ---
void view3d_render(Canvas& out, const Map& m, const Camera& cam, const Billboard* bot, float light) {
    const float dirX = cosf(cam.angle), dirY = sinf(cam.angle);
    // camera plane points to the viewer's right (map y grows downwards)
    const float plX = -dirY * PLANE, plY = dirX * PLANE;
    const float horizon = VIEW_H / 2.0f + cam.bob;

    // ---- floor and ceiling (row by row) ----
    const float rdLX = dirX - plX, rdLY = dirY - plY, rdRX = dirX + plX, rdRY = dirY + plY;
    for (int y = 0; y < VIEW_H; y++) {
        float p = (float)y + 0.5f - horizon;
        bool floorRow = p > 0;
        if (!floorRow) p = -p;
        if (p < 0.5f) p = 0.5f;
        float rowDist = 0.5f * K / p;
        float stepX = rowDist * (rdRX - rdLX) / VIEW_W, stepY = rowDist * (rdRY - rdLY) / VIEW_W;
        float fx = cam.x + rowDist * rdLX, fy = cam.y + rowDist * rdLY;
        uint32_t* row = s_view + y * VIEW_W;
        for (int x = 0; x < VIEW_W; x++, fx += stepX, fy += stepY) {
            int cx = (int)floorf(fx), cy = (int)floorf(fy);
            int tx = (int)(fx * TEX) & (TEX - 1), ty = (int)(fy * TEX) & (TEX - 1);
            bool lit = is_light_cell(cx, cy);
            uint32_t c;
            if (floorRow) c = fog_of((lit ? s_floorLit : s_floorTex)[ty * TEX + tx], rowDist, light);
            else if (lit) c = fog_of(s_lightTex[ty * TEX + tx], rowDist * 0.45f, light); // lights glow through the fog
            else c = fog_of(s_ceilTex[ty * TEX + tx], rowDist, light);
            row[x] = c;
        }
    }

    // ---- walls (one ray per column, DDA) ----
    for (int x = 0; x < VIEW_W; x++) {
        float camX = 2.0f * x / VIEW_W - 1.0f;
        float rdX = dirX + plX * camX, rdY = dirY + plY * camX;
        int mx = (int)floorf(cam.x), my = (int)floorf(cam.y);
        float ddX = rdX == 0 ? 1e30f : fabsf(1.0f / rdX), ddY = rdY == 0 ? 1e30f : fabsf(1.0f / rdY);
        int stX, stY;
        float sdX, sdY;
        if (rdX < 0) { stX = -1; sdX = (cam.x - mx) * ddX; } else { stX = 1; sdX = (mx + 1.0f - cam.x) * ddX; }
        if (rdY < 0) { stY = -1; sdY = (cam.y - my) * ddY; } else { stY = 1; sdY = (my + 1.0f - cam.y) * ddY; }
        int side = 0, guard = 0;
        while (guard++ < 256) {
            if (sdX < sdY) { sdX += ddX; mx += stX; side = 0; }
            else { sdY += ddY; my += stY; side = 1; }
            if (map_wall(m, mx, my)) break;
        }
        float dist = side == 0 ? sdX - ddX : sdY - ddY;
        if (dist < 0.02f) dist = 0.02f;
        s_zbuf[x] = dist;

        float wallX = side == 0 ? cam.y + dist * rdY : cam.x + dist * rdX;
        wallX -= floorf(wallX);
        int texX = (int)(wallX * TEX);
        if ((side == 0 && rdX < 0) || (side == 1 && rdY > 0)) texX = TEX - 1 - texX; // no mirrored textures
        texX &= TEX - 1;

        float lineH = K / dist;
        float top = horizon - lineH * 0.5f;
        int y0 = (int)ceilf(top), y1 = (int)ceilf(horizon + lineH * 0.5f);
        if (y0 < 0) y0 = 0;
        if (y1 > VIEW_H) y1 = VIEW_H;
        const uint32_t* tex = side ? s_wallDark : s_wallTex;
        float texStep = TEX / lineH;
        float texPos = (y0 - top) * texStep;
        // fog and light are constant along the column
        int fi = (int)(dist * 16.0f);
        uint32_t f = s_fog[fi < FOG_STEPS ? fi : FOG_STEPS - 1];
        uint32_t lm = (uint32_t)(light * 256.0f);
        for (int y = y0; y < y1; y++, texPos += texStep) {
            int ty = (int)texPos;
            if (ty > TEX - 1) ty = TEX - 1;
            uint32_t c = tex[ty * TEX + texX];
            if (lm < 256) c = shade(c, lm);
            s_view[y * VIEW_W + x] = f ? blend(c, s_theme.fog, f) : c;
        }
    }

    // ---- the bot: billboard, occluded column by column ----
    s_botVisible = false;
    if (bot && bot->sprite && bot->sprite->px) {
        float sx = bot->x - cam.x, sy = bot->y - cam.y;
        float depth = sx * dirX + sy * dirY;                    // along the view direction
        float lateral = (sx * plX + sy * plY) / (PLANE * PLANE); // in camera-plane units
        if (depth > 0.1f) {
            float screenX = (1.0f + lateral / depth) * VIEW_W * 0.5f;
            const Sprite& sp = *bot->sprite;
            float hPx = bot->height * K / depth;
            float wPx = hPx * sp.w / sp.h;
            float bottom = horizon + (0.5f - bot->lift) * K / depth;  // floor level minus hover
            float top = bottom - hPx;
            int x0 = (int)(screenX - wPx * 0.5f), x1 = (int)(screenX + wPx * 0.5f);
            int y0 = (int)top, y1 = (int)bottom;
            int fi = (int)(depth * 16.0f);
            uint32_t f = s_fog[fi < FOG_STEPS ? fi : FOG_STEPS - 1];
            if (f > 200) f = 200;                                 // keep a silhouette in the dark
            int cxs = (int)screenX;
            if (cxs >= 0 && cxs < VIEW_W && depth < s_zbuf[cxs]) s_botVisible = true;
            for (int x = x0 < 0 ? 0 : x0; x < x1 && x < VIEW_W; x++) {
                if (depth >= s_zbuf[x]) continue; // behind a wall
                int tx = (int)((x - (screenX - wPx * 0.5f)) * sp.w / wPx);
                if (tx < 0 || tx >= sp.w) continue;
                for (int y = y0 < 0 ? 0 : y0; y < y1 && y < VIEW_H; y++) {
                    int ty = (int)((y - top) * sp.h / hPx);
                    if (ty < 0 || ty >= sp.h) continue;
                    uint32_t p = sp.px[ty * sp.w + tx];
                    uint32_t a = p >> 24;
                    if (a < 16) continue;
                    if (f) p = blend(p, s_theme.fog, f);
                    uint32_t* d = &s_view[y * VIEW_W + x];
                    *d = a >= 240 ? p : blend(*d, p, a);
                }
            }
        }
    }

    // ---- 2x upscale into the canvas ----
    for (int y = 0; y < VIEW_H; y++) {
        const uint32_t* s = s_view + y * VIEW_W;
        uint32_t* d0 = out.px + (size_t)(y * 2) * out.w;
        uint32_t* d1 = d0 + out.w;
        for (int x = 0; x < VIEW_W; x++) { uint32_t c = s[x]; d0[2 * x] = c; d0[2 * x + 1] = c; }
        memcpy(d1, d0, (size_t)VIEW_W * 2 * 4);
    }
}
