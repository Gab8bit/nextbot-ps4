// PS4 video output via libSceVideoOut (same setup that runs on real hardware in Pixo Adventure).
#include "video.h"

#include <stdio.h>
#include <string.h>
#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>

#include "../gfx/draw.h"

static const int FB_W = 1920, FB_H = 1080;
static_assert(FB_W == SCREEN_W * 2 && FB_H == SCREEN_H * 2, "canvas must be half the framebuffer");

static int s_video = -1;
static OrbisKernelEqueue s_flipQueue;
static bool s_queueOk = false;
static uint32_t* s_fb[2];
static int s_cur = 0;
static int64_t s_flipArg = 0;

bool video_init() {
    s_video = sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN, ORBIS_VIDEO_OUT_BUS_MAIN, 0, 0);
    if (s_video < 0) { printf("[nextbot] sceVideoOutOpen failed: %x\n", s_video); return false; }

    if (sceKernelCreateEqueue(&s_flipQueue, "nextbot flip") < 0) {
        printf("[nextbot] sceKernelCreateEqueue failed\n");
        return false;
    }
    s_queueOk = true;
    sceVideoOutAddFlipEvent(s_flipQueue, s_video, 0);

    // Framebuffers live in direct memory, 2 MB aligned.
    const size_t align = 0x200000;
    const size_t fbSize = (size_t)FB_W * FB_H * 4;
    const size_t total = (fbSize * 2 + align - 1) / align * align;
    off_t phys = 0;
    if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), total, align, 3, &phys) < 0) {
        printf("[nextbot] direct memory alloc failed\n");
        return false;
    }
    void* mem = NULL;
    if (sceKernelMapDirectMemory(&mem, total, 0x33, 0, phys, align) < 0) {
        printf("[nextbot] direct memory map failed\n");
        return false;
    }
    s_fb[0] = (uint32_t*)mem;
    s_fb[1] = (uint32_t*)((char*)mem + fbSize);
    memset(mem, 0, fbSize * 2);

    OrbisVideoOutBufferAttribute attr;
    // 0x80000000 = A8R8G8B8 sRGB, tiling mode 1 = linear
    sceVideoOutSetBufferAttribute(&attr, 0x80000000, 1, 0, FB_W, FB_H, FB_W);
    if (sceVideoOutRegisterBuffers(s_video, 0, (void**)s_fb, 2, &attr) != 0) {
        printf("[nextbot] sceVideoOutRegisterBuffers failed\n");
        return false;
    }
    sceVideoOutSetFlipRate(s_video, 0); // 60 Hz
    return true;
}

void video_present(const uint32_t* src) {
    static uint32_t line[FB_W];
    uint32_t* dst = s_fb[s_cur];
    for (int y = 0; y < SCREEN_H; y++) {
        const uint32_t* s = src + y * SCREEN_W;
        for (int x = 0; x < SCREEN_W; x++) { uint32_t c = s[x]; line[x * 2] = c; line[x * 2 + 1] = c; }
        // the framebuffer is write-combined memory: write whole lines with memcpy, never read it back
        memcpy(dst + (size_t)(y * 2) * FB_W, line, sizeof(line));
        memcpy(dst + (size_t)(y * 2 + 1) * FB_W, line, sizeof(line));
    }

    sceVideoOutSubmitFlip(s_video, s_cur, ORBIS_VIDEO_OUT_FLIP_VSYNC, s_flipArg);

    // wait until this flip is on screen; then the other buffer is free to draw into
    for (;;) {
        OrbisVideoOutFlipStatus st;
        sceVideoOutGetFlipStatus(s_video, &st);
        if (st.flipArg == s_flipArg) break;
        OrbisKernelEvent ev;
        int n;
        if (sceKernelWaitEqueue(s_flipQueue, &ev, 1, &n, 0) != 0) break;
    }
    s_flipArg++;
    s_cur ^= 1;
}

void video_shutdown() {
    if (s_video >= 0) sceVideoOutClose(s_video);
    if (s_queueOk) sceKernelDeleteEqueue(s_flipQueue);
    s_video = -1;
    s_queueOk = false;
}
