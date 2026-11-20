// Video output: two 1920x1080 framebuffers (double buffering), flip on vsync.
#pragma once
#include <stdint.h>

bool video_init();
// Scales the 960x540 game canvas 2x into the back buffer, flips on vsync and waits
// until the flip has happened (this paces the game loop at 60 fps).
void video_present(const uint32_t* canvas);
void video_shutdown();
