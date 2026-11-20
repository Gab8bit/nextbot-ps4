#include "system.h"

#include <orbis/libkernel.h>
#include <orbis/SystemService.h>

uint64_t system_time_us() { return sceKernelGetProcessTime(); }
void system_sleep_us(uint32_t us) { sceKernelUsleep(us); }
void system_hide_splash() { sceSystemServiceHideSplashScreen(); }
