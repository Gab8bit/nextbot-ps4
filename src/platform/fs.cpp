// Portable POSIX implementation (OpenOrbis libc on PS4, glibc in the PC test harness).
#include "fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef NEXTBOT_PS4
#include <orbis/libkernel.h>
#endif

static char s_root[FS_PATH_MAX] = "/data/nextbot";

const char* fs_root() { return s_root; }
void fs_set_root(const char* root) { snprintf(s_root, sizeof(s_root), "%s", root); }

bool fs_path(char* buf, int len, const char* sub) {
    int n = snprintf(buf, len, "%s/%s", s_root, sub);
    return n > 0 && n < len;
}

bool fs_is_dir(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool fs_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

bool fs_ensure_dir(const char* path) {
    if (fs_is_dir(path)) return true;
    if (mkdir(path, 0777) == 0) return true;
#ifdef NEXTBOT_PS4
    // fall back to the kernel call in case the libc wrapper is not available
    if (sceKernelMkdir(path, 0777) == 0) return true;
#endif
    return fs_is_dir(path);
}

// If name ends with ext (case-insensitive), copies the base name into out and returns true.
static bool match_ext(const char* name, const char* ext, FsName* out) {
    size_t n = strlen(name), e = strlen(ext);
    if (n <= e || n - e >= (size_t)FS_NAME_MAX) return false;
    if (strcasecmp(name + n - e, ext) != 0) return false;
    if (name[0] == '.') return false; // hidden files (e.g. macOS "._shrek.png")
    memcpy(out->name, name, n - e);
    out->name[n - e] = 0;
    snprintf(out->file, sizeof(out->file), "%s", name);
    return true;
}

static int cmp_names(const void* a, const void* b) {
    return strcasecmp(((const FsName*)a)->name, ((const FsName*)b)->name);
}

int fs_list(const char* dir, const char* ext, FsName* out, int max) {
    int count = 0;
    DIR* d = opendir(dir);
    if (d) {
        struct dirent* e;
        while ((e = readdir(d)) != NULL && count < max) {
            if (e->d_type == DT_DIR) continue;
            if (match_ext(e->d_name, ext, &out[count])) count++;
        }
        closedir(d);
    } else {
#ifdef NEXTBOT_PS4
        // Fallback: raw getdents on the directory (FreeBSD dirent layout).
        int fd = sceKernelOpen(dir, O_RDONLY | O_DIRECTORY, 0);
        if (fd < 0) return -1;
        static char buf[4096];
        int n;
        while ((n = sceKernelGetdents(fd, buf, sizeof(buf))) > 0 && count < max) {
            int off = 0;
            while (off < n && count < max) {
                struct dirent* e = (struct dirent*)(buf + off);
                if (e->d_reclen == 0) break;
                if (e->d_type != DT_DIR && match_ext(e->d_name, ext, &out[count])) count++;
                off += e->d_reclen;
            }
        }
        sceKernelClose(fd);
#else
        return -1;
#endif
    }
    qsort(out, count, sizeof(FsName), cmp_names);
    return count;
}

bool fs_read_file(const char* path, size_t maxLen, unsigned char** data, size_t* len) {
    *data = NULL; *len = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    long size = ftell(f);
    if (size < 0 || (size_t)size > maxLen) { fclose(f); return false; }
    fseek(f, 0, SEEK_SET);
    unsigned char* buf = (unsigned char*)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return false; }
    size_t got = size > 0 ? fread(buf, 1, (size_t)size, f) : 0;
    fclose(f);
    if (got != (size_t)size) { free(buf); return false; }
    buf[size] = 0; // handy for text files
    *data = buf; *len = (size_t)size;
    return true;
}

bool fs_write_atomic(const char* path, const void* data, size_t len) {
    char tmp[FS_PATH_MAX];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) return false;
    FILE* f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = len == 0 || fwrite(data, 1, len, f) == len;
    ok = (fflush(f) == 0) && ok;
    if (ok) fsync(fileno(f)); // make sure the data is on disk before the rename
    ok = (fclose(f) == 0) && ok;
    if (!ok) { unlink(tmp); return false; }
    if (rename(tmp, path) != 0) {
        // some file systems refuse to rename over an existing file
        unlink(path);
        if (rename(tmp, path) != 0) { unlink(tmp); return false; }
    }
    return true;
}

bool fs_remove(const char* path) {
    if (unlink(path) == 0) return true;
    return !fs_exists(path);
}
