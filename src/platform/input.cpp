#include "input.h"

#include <stdio.h>
#include <string.h>
#include <orbis/Pad.h>

static int s_pad = -1;
static uint32_t s_lastLight = 0xFFFFFFFFu;
static int s_lastBig = -1, s_lastSmall = -1;

bool input_init(int userId) {
    if (scePadInit() != 0) { printf("[nextbot] scePadInit failed\n"); return false; }
    s_pad = scePadOpen(userId, 0, 0, NULL);
    if (s_pad < 0) { printf("[nextbot] scePadOpen failed: %x\n", s_pad); return false; }
    return true;
}

static float stick_axis(uint8_t v) {
    float f = ((float)v - 128.0f) / 127.0f;
    if (f < -1.0f) f = -1.0f;
    if (f > 1.0f) f = 1.0f;
    return f;
}

void input_read(PadRaw* out) {
    memset(out, 0, sizeof(*out));
    if (s_pad < 0) return;
    OrbisPadData d;
    if (scePadReadState(s_pad, &d) != 0 || !d.connected) return;
    out->connected = true;
    uint32_t b = d.buttons, r = 0;
    if (b & ORBIS_PAD_BUTTON_UP)       r |= PAD_UP;
    if (b & ORBIS_PAD_BUTTON_DOWN)     r |= PAD_DOWN;
    if (b & ORBIS_PAD_BUTTON_LEFT)     r |= PAD_LEFT;
    if (b & ORBIS_PAD_BUTTON_RIGHT)    r |= PAD_RIGHT;
    if (b & ORBIS_PAD_BUTTON_CROSS)    r |= PAD_CROSS;
    if (b & ORBIS_PAD_BUTTON_CIRCLE)   r |= PAD_CIRCLE;
    if (b & ORBIS_PAD_BUTTON_SQUARE)   r |= PAD_SQUARE;
    if (b & ORBIS_PAD_BUTTON_TRIANGLE) r |= PAD_TRIANGLE;
    if (b & ORBIS_PAD_BUTTON_L1)       r |= PAD_L1;
    if (b & ORBIS_PAD_BUTTON_R1)       r |= PAD_R1;
    if (b & ORBIS_PAD_BUTTON_L2)       r |= PAD_L2;
    if (b & ORBIS_PAD_BUTTON_R2)       r |= PAD_R2;
    if (b & ORBIS_PAD_BUTTON_OPTIONS)  r |= PAD_OPTIONS;
    out->buttons = r;
    out->lx = stick_axis(d.leftStick.x);
    out->ly = stick_axis(d.leftStick.y);
    out->rx = stick_axis(d.rightStick.x);
    out->ry = stick_axis(d.rightStick.y);
    out->l2 = d.analogButtons.l2 / 255.0f;
    out->r2 = d.analogButtons.r2 / 255.0f;
}

void input_set_feedback(uint32_t light, uint8_t big, uint8_t small) {
    if (s_pad < 0) return;
    if (light != s_lastLight) {
        OrbisPadColor col = { (uint8_t)(light >> 16), (uint8_t)(light >> 8), (uint8_t)light, 255 };
        scePadSetLightBar(s_pad, &col);
        s_lastLight = light;
    }
    if (big != s_lastBig || small != s_lastSmall) {
        OrbisPadVibeParam v = { big, small };
        scePadSetVibration(s_pad, &v);
        s_lastBig = big; s_lastSmall = small;
    }
}

void input_shutdown() {
    if (s_pad < 0) return;
    OrbisPadVibeParam v = { 0, 0 };
    scePadSetVibration(s_pad, &v);
    scePadResetLightBar(s_pad);
    scePadClose(s_pad);
    s_pad = -1;
}
