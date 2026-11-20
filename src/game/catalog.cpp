#include "catalog.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

static BotEntry s_bots[CATALOG_MAX];
static int s_count = 0;
static bool s_foldersOk = false;
static FsName s_names[CATALOG_MAX];
static FsName s_sounds[256];

bool catalog_ensure_folders() {
    char p[FS_PATH_MAX];
    bool ok = fs_ensure_dir(fs_root());
    if (fs_path(p, sizeof(p), "png")) ok = fs_ensure_dir(p) && ok;
    if (fs_path(p, sizeof(p), "sounds")) ok = fs_ensure_dir(p) && ok;
    s_foldersOk = ok;
    if (!ok) printf("[nextbot] could not create the folders under %s\n", fs_root());
    return ok;
}

bool catalog_folders_ok() { return s_foldersOk; }

void catalog_free() {
    for (int i = 0; i < s_count; i++) { sprite_free(&s_bots[i].thumb); sprite_free(&s_bots[i].preview); }
    s_count = 0;
}

int catalog_scan() {
    catalog_free();
    catalog_ensure_folders();
    char dir[FS_PATH_MAX];
    if (!fs_path(dir, sizeof(dir), "png")) return 0;
    int n = fs_list(dir, ".png", s_names, CATALOG_MAX);
    if (n < 0) n = 0;
    int ns = 0;
    if (fs_path(dir, sizeof(dir), "sounds")) ns = fs_list(dir, ".wav", s_sounds, 256);
    if (ns < 0) ns = 0;

    for (int i = 0; i < n; i++) {
        BotEntry* b = &s_bots[s_count];
        memset(b, 0, sizeof(*b));
        snprintf(b->name, sizeof(b->name), "%s", s_names[i].name);
        snprintf(b->pngFile, sizeof(b->pngFile), "%s", s_names[i].file);
        // exact match first, then case-insensitive ("Shrek.WAV" for "shrek.png")
        for (int pass = 0; pass < 2 && !b->hasSound; pass++)
            for (int k = 0; k < ns && !b->hasSound; k++) {
                bool same = pass == 0 ? strcmp(s_sounds[k].name, b->name) == 0
                                      : strcasecmp(s_sounds[k].name, b->name) == 0;
                if (same) { snprintf(b->wavFile, sizeof(b->wavFile), "%s", s_sounds[k].file); b->hasSound = true; }
            }
        s_count++;

        // Decode once just to build the previews, then drop the full image.
        char path[FS_PATH_MAX];
        if (!catalog_png_path(s_count - 1, path, sizeof(path))) continue;
        Sprite full;
        if (sprite_load_png(path, &full, b->error, sizeof(b->error))) {
            if (!sprite_resize_fit(full, THUMB_SIZE, THUMB_SIZE, &b->thumb) ||
                !sprite_resize_fit(full, PREVIEW_SIZE, PREVIEW_SIZE, &b->preview))
                snprintf(b->error, sizeof(b->error), "memoria insufficiente");
            sprite_free(&full);
        } else {
            printf("[nextbot] %s: %s\n", path, b->error);
        }
    }
    return s_count;
}

int catalog_count() { return s_count; }
const BotEntry* catalog_get(int i) { return (i >= 0 && i < s_count) ? &s_bots[i] : nullptr; }

bool catalog_png_path(int i, char* buf, int len) {
    if (i < 0 || i >= s_count) return false;
    int r = snprintf(buf, len, "%s/png/%s", fs_root(), s_bots[i].pngFile);
    return r > 0 && r < len;
}

bool catalog_wav_path(int i, char* buf, int len) {
    if (i < 0 || i >= s_count || !s_bots[i].hasSound) return false;
    int r = snprintf(buf, len, "%s/sounds/%s", fs_root(), s_bots[i].wavFile);
    return r > 0 && r < len;
}
