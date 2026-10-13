#pragma once

// Hardware abstraction that main/ui/*.c build against, so the same source
// compiles unchanged for the real board (board_esp32.c, driving
// esp_lcd/i2c/adc directly - no vendor BSP exists for this board) and for
// the desktop simulator (sim/board_sim.c, backed by SDL2). Anything the UI
// needs from the outside world goes here instead of including
// esp_lcd/FreeRTOS/SDL headers directly.

#include <stdbool.h>
#include <stdint.h>

// Hardware facts (pinout, panel resolution) live in one place under
// config/, shared by the real firmware and the simulator.
#include "../../config/board_config.h"

#define BOARD_WIFI_SCAN_MAX_APS 10

typedef struct {
    char ssid[33];
    int8_t rssi;
} board_wifi_ap_t;

// One-time setup, call before any other board_* function.
void board_init(void);

// LCD backlight brightness, 0-100.
void board_set_backlight(uint8_t percent);

// Mounts the microSD card (if one is fitted) and makes it readable at
// "/sdcard". Returns false, harmlessly, if there is no card or it cannot be
// read: nothing in the product depends on it being there (the artwork it holds
// is a bonus; the UI falls back to drawing everything with shapes). Safe to
// call more than once. On the simulator this reports whether the asset folder
// exists.
bool board_sd_mount(void);

// Puts the panel itself to sleep (backlight off and the controller in its
// low-power sleep-in state) or wakes it again. Used by deep sleep, when
// nothing is on show. The panel keeps its last image across a sleep, and
// waking it takes about 100 ms. The backlight comes back at 0%: bring it up
// with board_set_backlight() afterwards.
void board_display_sleep(bool sleep);

// Onboard RGB LED (WS2812 on real hardware).
void board_set_led(uint8_t r, uint8_t g, uint8_t b);

// One buzz of the haptic motor: `ms` long (1-2000) at `strength_pct` (10-100) of
// full power, then it stops by itself. Returns immediately; a new call replaces
// one in progress. Safe from any task. The simulator just logs it.
void board_haptic_pulse(uint16_t ms, uint8_t strength_pct);

// Feedback buzzes that honour the user's vibration setting (onboarding_get_vibration):
// silent when vibration is off, and never cutting across a ring in progress.
//   TICK   - one soft tick: an option highlighted or chosen
//   NOTIFY - a soft double tap: something has come up (a joke, a reminder...)
typedef enum {
    BOARD_HAPTIC_TICK,
    BOARD_HAPTIC_NOTIFY,
} board_haptic_kind_t;
void board_haptic_feedback(board_haptic_kind_t kind);

// Guards LVGL calls made from outside the display's own update loop
// (e.g. from the task/thread a scan runs on). Call board_lock() before
// touching any lv_obj_t from such a context, board_unlock() after.
bool board_lock(uint32_t timeout_ms);
void board_unlock(void);

// Runs fn(ctx) on a background task/thread; fn must not block forever.
void board_run_async(void (*fn)(void *ctx), void *ctx);

// Stats shown on the Home tab.
uint32_t board_get_free_heap_kb(void);
uint32_t board_get_uptime_seconds(void);

// Blocking: scans for nearby WiFi networks, sorted by RSSI (strongest
// first). Never call from the display's own update loop - call it from a
// board_run_async() job and take board_lock() before publishing results.
void board_wifi_scan(board_wifi_ap_t *out_aps, uint16_t *out_count, uint16_t max_aps);

// Raw accelerometer reading in g. Real hardware: QMI8658 (no touch panel
// on this board, so tilt is the gesture input instead - see
// main/ui/gesture.c). Simulator: arrow keys synthesize a fake tilt.
void board_get_accel(float *x, float *y, float *z);

// True at most once per physical press-and-release of the primary button
// (BOOT on real hardware, Space in the simulator). Consumes the edge, so
// call it from a single place (main/ui/gesture.c) at a steady poll rate.
bool board_button_clicked(void);

// True at most once per long press (held about 1.5 s) of the same button;
// consumes the event. A long press is reported instead of, never in addition
// to, a click. Real hardware: BOOT held. Simulator: the F key.
bool board_button_long_pressed(void);

// 0-100 battery charge, or -1 if no battery is present/measurable.
int board_get_battery_pct(void);

// True while the battery is being charged / the board is on USB power. There is
// no charge-status pin, so it is inferred (see board_esp32.c): a computer's USB
// link, the battery pin sitting at the charger's level, or a steadily rising
// voltage. Call board_get_battery_pct() regularly (it keeps the history this
// uses). While charging, the percentage is only roughly right - the pin reads
// above the cell's own voltage.
bool board_battery_charging(void);

// Signal strength in dBm of the access point the board is currently
// associated with (about -30 strong .. -90 weak), or 0 when not connected.
int board_get_wifi_rssi(void);

// Starts the USB serial listener that accepts files for the SD card
// (tools/sd_push.py). Firmware only; the simulator reads its card directly.
void sd_upload_start(void);
