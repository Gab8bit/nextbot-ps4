// Nextbot - PS4 homebrew (OpenOrbis).
// main: system init, 60 fps loop with delta time, shutdown. The state machine
// (MENU / PLAYING / GAME_OVER / LEADERBOARD / SETTINGS) lives in game/app.cpp so
// the PC test harness can drive exactly the same code.
#include <stdio.h>
#include <stdlib.h>

#include "gfx/draw.h"
#include "game/app.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/system.h"
#include "platform/user.h"
#include "platform/video.h"

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0); // logs visible immediately (klog / debug console)
    printf("[nextbot] starting\n");

    int userId = user_init();
    char userName[64];
    if (!user_get_name(userId, userName, sizeof(userName))) printf("[nextbot] user name unavailable, using fallback\n");
    printf("[nextbot] user %d: %s\n", userId, userName);

    input_init(userId);

    if (!video_init()) {
        printf("[nextbot] video init failed, cannot continue\n");
        for (;;) system_sleep_us(1000000);
    }
    if (!audio_init()) printf("[nextbot] audio disabled\n");
    system_hide_splash();

    static uint32_t pixels[SCREEN_W * SCREEN_H];
    Canvas canvas = { pixels, SCREEN_W, SCREEN_H };
    app_init(userName);

    uint64_t last = system_time_us();
    for (;;) { // the app is closed from the PS button / home screen, the loop never ends
        uint64_t now = system_time_us();
        float dt = (float)(now - last) / 1000000.0f;
        last = now;
        if (dt <= 0 || dt > 0.1f) dt = 1.0f / 60.0f;

        PadRaw pad;
        input_read(&pad);
        app_frame(pad, dt, canvas);
        video_present(pixels); // waits for vsync -> 60 fps
    }

    app_shutdown();
    audio_shutdown();
    input_shutdown();
    video_shutdown();
    user_shutdown();
    return 0;
}
