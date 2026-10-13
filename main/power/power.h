#pragma once

#include <stdbool.h>

// Deep sleep: what the device does after the character has been asleep for a
// few minutes (character.h decides when). The screen has been showing a dim,
// closed-eyed face; now nothing needs to be on show at all, so:
//
//   - the panel goes into its sleep-in state with the backlight off - by far
//     the biggest saving, the backlight being most of the device's draw;
//   - Wi-Fi stays connected but listens about once a second instead of every
//     beacon, so it is still online the moment the device wakes;
//   - the chip enters automatic light sleep, waking only when something is
//     due: the character's, the motion poll's and the scheduler's slowed
//     timers (a few times a second). CPU, RAM and the Wi-Fi association are
//     kept, so a wake is instant - no reboot, nothing reconnects.
//
// The QMI8658's interrupt pin is not wired to the chip on this board (none is
// in config/board_config.h), so there is no wake-on-motion interrupt: the
// motion poll itself is what notices a pick-up, at a few polls a second.
//
// The power manager is also what makes light sleep *possible* at all:
// power_init() enables it in the build. It does not sleep by itself.
void power_init(void);

// Enter / leave deep sleep. Called from the display task by the character
// engine. Safe to call repeatedly.
void power_deep_enter(void);
void power_deep_exit(void);

// While USB is plugged in to a computer the chip is kept awake even in deep
// sleep, because light sleep drops the USB serial connection (no logs, no
// flashing). The screen and Wi-Fi still save power. A phone charger or a
// battery is not a "host", so a real product sleeps fully. Set to 1 to sleep
// even with a computer attached (the serial port will vanish until wake).
#ifndef POWER_LIGHT_SLEEP_WITH_USB
#define POWER_LIGHT_SLEEP_WITH_USB 0
#endif
