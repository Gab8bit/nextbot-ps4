// File system helpers: folders under /data/nextbot, directory scan, whole-file read,
// atomic write (temp file + rename).
#pragma once
#include <stddef.h>

constexpr int FS_NAME_MAX = 128;   // max base-name length we accept (longer files are skipped)
constexpr int FS_PATH_MAX = 512;

struct FsName {
    char name[FS_NAME_MAX];      // base name without extension
    char file[FS_NAME_MAX + 8];  // real file name as found on disk
};

const char* fs_root();                 // "/data/nextbot" on PS4
void fs_set_root(const char* root);    // used by the PC test harness
// Builds "<root>/<sub>" into buf. Returns false if it does not fit.
bool fs_path(char* buf, int len, const char* sub);

bool fs_ensure_dir(const char* path);  // creates the folder if missing (parent must exist)
bool fs_is_dir(const char* path);
bool fs_exists(const char* path);

// Lists files in dir whose extension matches ext (case-insensitive, e.g. ".png").
// Writes base and real file names sorted case-insensitively; returns the count, -1 on error.
int fs_list(const char* dir, const char* ext, FsName* out, int max);

// Reads a whole file (up to maxLen bytes). *data must be freed with free(). False on error.
bool fs_read_file(const char* path, size_t maxLen, unsigned char** data, size_t* len);
// Writes "<path>.tmp", syncs it, then renames it over path.
bool fs_write_atomic(const char* path, const void* data, size_t len);
// Deletes a file. Returns true if it is gone (also when it did not exist).
bool fs_remove(const char* path);
