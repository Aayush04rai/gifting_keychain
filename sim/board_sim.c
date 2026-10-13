#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "board_hal.h"

#include "../config/sim_config.h"

static SDL_mutex *s_lock;
static Uint32 s_start_ticks;

void board_init(void)
{
    s_lock = SDL_CreateMutex();
    s_start_ticks = SDL_GetTicks();
}

// The simulator's "SD card" is the assets/sd folder in the project (LVGL's
// stdio file-system driver is pointed at it by sim/CMakeLists.txt), so it is
// always there.
bool board_sd_mount(void)
{
    return true;
}

void board_display_sleep(bool sleep)
{
    printf("[sim] display %s\n", sleep ? "SLEEP (backlight off, panel in sleep-in)" : "WAKE");
    fflush(stdout);
}

void board_set_backlight(uint8_t percent)
{
    printf("[sim] backlight -> %u%%\n", percent);
    fflush(stdout);
}

void board_haptic_pulse(uint16_t ms, uint8_t strength_pct)
{
    printf("[sim] haptic %u ms at %u%%\n", (unsigned)ms, (unsigned)strength_pct);
}

void board_haptic_feedback(board_haptic_kind_t kind)
{
    printf("[sim] haptic feedback: %s\n", kind == BOARD_HAPTIC_TICK ? "tick" : "notify");
}

void board_set_led(uint8_t r, uint8_t g, uint8_t b)
{
    printf("[sim] LED -> #%02X%02X%02X\n", r, g, b);
    fflush(stdout);
}

bool board_lock(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return SDL_LockMutex(s_lock) == 0;
}

void board_unlock(void)
{
    SDL_UnlockMutex(s_lock);
}

typedef struct {
    void (*fn)(void *ctx);
    void *ctx;
} async_job_t;

static int thread_trampoline(void *arg)
{
    async_job_t *job = arg;
    job->fn(job->ctx);
    free(job);
    return 0;
}

void board_run_async(void (*fn)(void *ctx), void *ctx)
{
    async_job_t *job = malloc(sizeof(async_job_t));
    if (!job) {
        return;
    }
    job->fn = fn;
    job->ctx = ctx;

    SDL_Thread *thread = SDL_CreateThread(thread_trampoline, "board_async", job);
    if (thread) {
        SDL_DetachThread(thread);
    } else {
        free(job);
    }
}

uint32_t board_get_free_heap_kb(void)
{
    return SIM_FAKE_FREE_HEAP_KB;
}

uint32_t board_get_uptime_seconds(void)
{
    return (SDL_GetTicks() - s_start_ticks) / 1000;
}

void board_wifi_scan(board_wifi_ap_t *out_aps, uint16_t *out_count, uint16_t max_aps)
{
    static const struct {
        const char *ssid;
        int8_t rssi;
    } fake_aps[] = {
        SIM_WIFI_FAKE_AP_LIST
    };

    SDL_Delay(SIM_WIFI_SCAN_DELAY_MS);

    uint16_t count = sizeof(fake_aps) / sizeof(fake_aps[0]);
    if (count > max_aps) {
        count = max_aps;
    }
    for (uint16_t i = 0; i < count; i++) {
        snprintf(out_aps[i].ssid, sizeof(out_aps[i].ssid), "%s", fake_aps[i].ssid);
        out_aps[i].rssi = fake_aps[i].rssi;
    }
    *out_count = count;
}

// No IMU on a desktop; Left/Right arrow keys fake a tilt on the same axis
// gesture.c watches. SDL_GetKeyboardState reflects live key state (kept up
// to date by lv_sdl_window's own event pump), so this needs no event loop
// of its own.
void board_get_accel(float *x, float *y, float *z)
{
    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    // Same axis layout as the real board's IMU: it is mounted for portrait,
    // so a sideways tilt of the landscape screen lands on Y and a
    // top/bottom tilt lands on X (see gesture.c).
    // Left/Right: gentle sideways tilt.
    *y = keys[SDL_SCANCODE_RIGHT] ? 1.0f : (keys[SDL_SCANCODE_LEFT] ? -1.0f : 0.0f);
    // Up/Down fake a sudden jerk (a big change in one poll), which gesture.c
    // reads as a shake.
    *x = keys[SDL_SCANCODE_DOWN] ? 3.0f : (keys[SDL_SCANCODE_UP] ? -3.0f : 0.0f);
    *z = 1.0f;

    // SIM_FACEDOWN="<from_ms>,<to_ms>" lays the simulated device face-down
    // (z = -1 g) for that stretch of the run, so the face-down focus gesture
    // can be exercised without a real IMU or keyboard injection.
    static int s_from = -1, s_to = -1;
    static bool s_parsed;
    if (!s_parsed) {
        s_parsed = true;
        const char *v = getenv("SIM_FACEDOWN");
        if (v) {
            sscanf(v, "%d,%d", &s_from, &s_to);
        }
    }
    // SIM_SHAKE_AT=<ms>: for half a second from then, the device is shaken
    // back and forth (x swings +-2 g every 100 ms), to check the shake
    // detector without keyboard injection.
    static int s_shake_at = -2;
    if (s_shake_at == -2) {
        const char *sv = getenv("SIM_SHAKE_AT");
        s_shake_at = sv ? atoi(sv) : -1;
    }
    int ts = (int)(SDL_GetTicks() - s_start_ticks);
    if (s_shake_at >= 0 && ts >= s_shake_at && ts < s_shake_at + 500) {
        *x = (((ts - s_shake_at) / 100) % 2) ? 2.0f : -2.0f;
    }

    // A hand takes about 0.6 s to turn a device over, so ramp rather than jump
    // (an instant 2 g step would look like a knock, which no real flip is).
    int t = (int)(SDL_GetTicks() - s_start_ticks);
    if (s_from >= 0) {
        const int ramp = 600;
        if (t >= s_from && t < s_from + ramp) {
            *z = 1.0f - 2.0f * (float)(t - s_from) / ramp;
        } else if (t >= s_from + ramp && t < s_to) {
            *z = -1.0f;
        } else if (t >= s_to && t < s_to + ramp) {
            *z = -1.0f + 2.0f * (float)(t - s_to) / ramp;
        }
    }
}

// Space bar stands in for the BOOT button; edge-detected here since
// board_button_clicked() itself must report each press only once.
bool board_button_clicked(void)
{
    static bool s_was_down;
    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    bool down = keys[SDL_SCANCODE_SPACE];
    bool clicked = down && !s_was_down;
    s_was_down = down;
    return clicked;
}

// The F key stands in for a long press of the BOOT button.
bool board_button_long_pressed(void)
{
    static bool s_was_down;
    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    bool down = keys[SDL_SCANCODE_F];
    bool pressed = down && !s_was_down;
    s_was_down = down;
    return pressed;
}

int board_get_battery_pct(void)
{
    const char *v = getenv("SIM_BATTERY");
    return v ? atoi(v) : SIM_FAKE_BATTERY_PCT;
}

bool board_battery_charging(void)
{
    const char *v = getenv("SIM_CHARGING");
    return v && atoi(v) != 0;
}

int board_get_wifi_rssi(void)
{
    const char *v = getenv("SIM_RSSI");
    return v ? atoi(v) : SIM_FAKE_WIFI_RSSI;
}
