#include "sprite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_THREAD_LOCALS
#define STBI_ASSERT(x)
#define STB_IMAGE_IMPLEMENTATION
#include "../../third_party/stb_image.h"

static const int MAX_PNG_SIDE = 4096;

bool sprite_create(Sprite* s, int w, int h) {
    s->w = s->h = 0;
    s->px = nullptr;
    if (w <= 0 || h <= 0) return false;
    s->px = (uint32_t*)malloc((size_t)w * h * 4);
    if (!s->px) return false;
    s->w = w; s->h = h;
    return true;
}

void sprite_free(Sprite* s) {
    if (s->px) free(s->px);
    s->px = nullptr;
    s->w = s->h = 0;
}

bool sprite_load_png(const char* path, Sprite* out, char* err, int errLen) {
    out->w = out->h = 0; out->px = nullptr;
    int w = 0, h = 0, n = 0;
    if (!stbi_info(path, &w, &h, &n)) {
        snprintf(err, errLen, "PNG non valido (%s)", stbi_failure_reason() ? stbi_failure_reason() : "?");
        return false;
    }
    if (w > MAX_PNG_SIDE || h > MAX_PNG_SIDE) {
        snprintf(err, errLen, "PNG troppo grande (%dx%d, max %d)", w, h, MAX_PNG_SIDE);
        return false;
    }
    unsigned char* rgba = stbi_load(path, &w, &h, &n, 4);
    if (!rgba) {
        snprintf(err, errLen, "decodifica fallita (%s)", stbi_failure_reason() ? stbi_failure_reason() : "?");
        return false;
    }
    if (!sprite_create(out, w, h)) {
        stbi_image_free(rgba);
        snprintf(err, errLen, "memoria insufficiente");
        return false;
    }
    size_t count = (size_t)w * h;
    for (size_t i = 0; i < count; i++) {
        const unsigned char* p = rgba + i * 4;
        out->px[i] = ((uint32_t)p[3] << 24) | ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    }
    stbi_image_free(rgba);
    return true;
}

bool sprite_resize_fit(const Sprite& src, int boxW, int boxH, Sprite* out) {
    out->w = out->h = 0; out->px = nullptr;
    if (!src.px || src.w <= 0 || src.h <= 0 || boxW <= 0 || boxH <= 0) return false;
    float sx = (float)boxW / src.w, sy = (float)boxH / src.h;
    float s = sx < sy ? sx : sy;
    int w = (int)(src.w * s + 0.5f), h = (int)(src.h * s + 0.5f);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w > boxW) w = boxW;
    if (h > boxH) h = boxH;
    if (!sprite_create(out, w, h)) return false;

    // Work in premultiplied alpha so transparent pixels do not bleed dark fringes.
    if (w <= src.w && h <= src.h) {
        // box filter: average every source pixel that falls in the destination pixel
        for (int dy = 0; dy < h; dy++) {
            int y0 = (int)((long long)dy * src.h / h), y1 = (int)((long long)(dy + 1) * src.h / h);
            if (y1 <= y0) y1 = y0 + 1;
            for (int dx = 0; dx < w; dx++) {
                int x0 = (int)((long long)dx * src.w / w), x1 = (int)((long long)(dx + 1) * src.w / w);
                if (x1 <= x0) x1 = x0 + 1;
                uint64_t ar = 0, ag = 0, ab = 0, aa = 0, cnt = 0;
                for (int y = y0; y < y1; y++) {
                    const uint32_t* row = src.px + (size_t)y * src.w;
                    for (int x = x0; x < x1; x++) {
                        uint32_t p = row[x], a = p >> 24;
                        ar += ((p >> 16) & 255) * a; ag += ((p >> 8) & 255) * a; ab += (p & 255) * a;
                        aa += a; cnt++;
                    }
                }
                uint32_t o = 0;
                if (aa > 0) {
                    uint32_t r = (uint32_t)(ar / aa), g = (uint32_t)(ag / aa), b = (uint32_t)(ab / aa);
                    uint32_t a = (uint32_t)(aa / cnt);
                    o = (a << 24) | (r << 16) | (g << 8) | b;
                }
                out->px[(size_t)dy * w + dx] = o;
            }
        }
    } else {
        // bilinear
        for (int dy = 0; dy < h; dy++) {
            float fy = ((float)dy + 0.5f) * src.h / h - 0.5f;
            if (fy < 0) fy = 0;
            int y0 = (int)fy; float ty = fy - y0;
            int y1 = y0 + 1 < src.h ? y0 + 1 : y0;
            for (int dx = 0; dx < w; dx++) {
                float fx = ((float)dx + 0.5f) * src.w / w - 0.5f;
                if (fx < 0) fx = 0;
                int x0 = (int)fx; float tx = fx - x0;
                int x1 = x0 + 1 < src.w ? x0 + 1 : x0;
                uint32_t p[4] = { src.px[(size_t)y0 * src.w + x0], src.px[(size_t)y0 * src.w + x1],
                                  src.px[(size_t)y1 * src.w + x0], src.px[(size_t)y1 * src.w + x1] };
                float wt[4] = { (1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty };
                float r = 0, g = 0, b = 0, a = 0;
                for (int k = 0; k < 4; k++) {
                    float pa = (float)(p[k] >> 24) * wt[k];
                    a += pa;
                    r += ((p[k] >> 16) & 255) * pa; g += ((p[k] >> 8) & 255) * pa; b += (p[k] & 255) * pa;
                }
                uint32_t o = 0;
                if (a > 0.5f) {
                    o = ((uint32_t)(a + 0.5f) << 24) | ((uint32_t)(r / a) << 16) |
                        ((uint32_t)(g / a) << 8) | (uint32_t)(b / a);
                }
                out->px[(size_t)dy * w + dx] = o;
            }
        }
    }
    return true;
}

static void blit_impl(Canvas& c, const Sprite& s, int x, int y, int alpha, uint32_t tint, int tintAmount) {
    if (!s.px || alpha <= 0) return;
    int sx0 = x < 0 ? -x : 0, sy0 = y < 0 ? -y : 0;
    int sx1 = s.w, sy1 = s.h;
    if (x + sx1 > c.w) sx1 = c.w - x;
    if (y + sy1 > c.h) sy1 = c.h - y;
    if (sx0 >= sx1 || sy0 >= sy1) return;
    for (int j = sy0; j < sy1; j++) {
        const uint32_t* sp = s.px + (size_t)j * s.w;
        uint32_t* dp = c.px + (size_t)(y + j) * c.w + x;
        for (int i = sx0; i < sx1; i++) {
            uint32_t p = sp[i];
            uint32_t a = p >> 24;
            if (a == 0) continue;
            if (tintAmount > 0) p = blend(p, tint, (uint32_t)tintAmount);
            if (alpha < 255) a = a * (uint32_t)alpha / 255;
            dp[i] = a >= 255 ? (p | 0xFF000000u) : blend(dp[i], p, a);
        }
    }
}

void sprite_blit(Canvas& c, const Sprite& s, int x, int y, int alpha) {
    blit_impl(c, s, x, y, alpha, 0, 0);
}

void sprite_blit_tint(Canvas& c, const Sprite& s, int x, int y, uint32_t tint, int tintAmount) {
    blit_impl(c, s, x, y, 255, tint, tintAmount);
}
