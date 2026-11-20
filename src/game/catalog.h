// Bot catalog: scans <root>/png for *.png and pairs each with <root>/sounds/<name>.wav.
#pragma once
#include "../gfx/sprite.h"
#include "../platform/fs.h"

constexpr int CATALOG_MAX = 64;
constexpr int THUMB_SIZE = 56;     // list icon
constexpr int PREVIEW_SIZE = 170;  // big preview of the selected bot

struct BotEntry {
    char name[FS_NAME_MAX];        // base name, e.g. "shrek"
    char pngFile[FS_NAME_MAX + 8]; // real file names on disk
    char wavFile[FS_NAME_MAX + 8]; // empty if there is no matching sound
    bool hasSound;
    Sprite thumb;                  // small preview, empty if the PNG could not be decoded
    Sprite preview;
    char error[96];                // why the PNG could not be loaded, if it failed
};

// Creates the folders if needed. Returns false if they could not be created.
bool catalog_ensure_folders();
// Rescans the folders (frees the previous scan). Returns the number of bots.
int catalog_scan();
int catalog_count();
const BotEntry* catalog_get(int i);
bool catalog_png_path(int i, char* buf, int len);
bool catalog_wav_path(int i, char* buf, int len);
void catalog_free();
bool catalog_folders_ok();   // result of the last folder check
