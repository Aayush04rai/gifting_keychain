#include <stdio.h>

#include "board_hal.h"
#include "power.h"

// The desktop simulator has no chip to put to sleep: it only shows the
// screen side (see board_sim.c) and says when deep sleep starts and ends.
static bool s_deep;

void power_init(void)
{
}

void power_deep_enter(void)
{
    if (s_deep) {
        return;
    }
    s_deep = true;
    board_display_sleep(true);
    printf("[sim] deep sleep: screen off, Wi-Fi low-power, light sleep (not simulated)\n");
    fflush(stdout);
}

void power_deep_exit(void)
{
    if (!s_deep) {
        return;
    }
    s_deep = false;
    board_display_sleep(false);
    printf("[sim] woke from deep sleep\n");
    fflush(stdout);
}
