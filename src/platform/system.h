// Misc system services: time, sleep, splash screen.
#pragma once
#include <stdint.h>

uint64_t system_time_us();          // monotonic microseconds
void system_sleep_us(uint32_t us);
void system_hide_splash();
