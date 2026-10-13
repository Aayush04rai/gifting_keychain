#include <stddef.h>
#include <stdint.h>

#include "esp_timer.h"

#include "board_hal.h"
#include "haptic_pattern.h"

typedef struct {
    uint16_t on_ms;
    uint8_t pct;
    uint16_t off_ms;
} step_t;

typedef struct {
    const char *name;
    const step_t *steps;
    int count;
    int loop_from; // where to resume after the last step
} pattern_t;

static const step_t k_double_tap[] = {{160, 100, 110}, {160, 100, 1400}};
static const step_t k_heartbeat[] = {{90, 70, 110}, {220, 100, 1250}};
static const step_t k_sonar[] = {{110, 100, 130}, {110, 100, 130}, {110, 100, 1500}};
// Builds to continuous firm buzzes within about 3 s, then holds.
static const step_t k_strongest[] = {{200, 70, 250}, {300, 85, 200}, {450, 100, 180}, {600, 100, 200}};
static const step_t k_urgent[] = {{100, 100, 90}, {100, 100, 90}, {100, 100, 90}, {100, 100, 90}, {100, 100, 900}};

static const step_t k_tick[] = {{45, 60, 0}};
static const step_t k_notify[] = {{110, 55, 120}, {110, 55, 0}};

static const pattern_t k_patterns[HAPTIC_PAT_COUNT] = {
    [HAPTIC_PAT_DOUBLE_TAP] = {"double-tap", k_double_tap, 2, 0},
    [HAPTIC_PAT_HEARTBEAT] = {"heartbeat", k_heartbeat, 2, 0},
    [HAPTIC_PAT_SONAR] = {"sonar", k_sonar, 3, 0},
    [HAPTIC_PAT_STRONGEST] = {"strongest", k_strongest, 4, 3},
    [HAPTIC_PAT_URGENT] = {"urgent", k_urgent, 5, 0},
    [HAPTIC_PAT_TICK] = {"tick", k_tick, 1, -1},
    [HAPTIC_PAT_NOTIFY] = {"notify", k_notify, 2, -1},
};

static esp_timer_handle_t s_timer;
static const pattern_t *s_cur;
static int s_idx;

static void play_step(void *arg)
{
    const pattern_t *p = s_cur;
    if (!p) {
        return;
    }
    const step_t *st = &p->steps[s_idx];
    board_haptic_pulse(st->on_ms, st->pct);
    uint64_t wait_us = (uint64_t)(st->on_ms + st->off_ms) * 1000u;
    s_idx++;
    if (s_idx >= p->count) {
        if (p->loop_from < 0) {
            s_cur = NULL; // a one-shot: this was its last step
            return;
        }
        s_idx = p->loop_from;
    }
    esp_timer_start_once(s_timer, wait_us);
}

void haptic_pattern_start(haptic_pattern_id_t id)
{
    if (id < 0 || id >= HAPTIC_PAT_COUNT) {
        return;
    }
    if (!s_timer) {
        const esp_timer_create_args_t args = {.callback = play_step, .name = "haptic_pat"};
        if (esp_timer_create(&args, &s_timer) != ESP_OK) {
            return;
        }
    }
    esp_timer_stop(s_timer);
    s_cur = &k_patterns[id];
    s_idx = 0;
    play_step(NULL);
}

void haptic_pattern_stop(void)
{
    s_cur = NULL;
    if (s_timer) {
        esp_timer_stop(s_timer);
    }
    board_haptic_pulse(1, 10); // cut a buzz in progress short
}

const char *haptic_pattern_name(haptic_pattern_id_t id)
{
    return (id >= 0 && id < HAPTIC_PAT_COUNT) ? k_patterns[id].name : "?";
}

bool haptic_pattern_looping(void)
{
    return s_cur != NULL && s_cur->loop_from >= 0;
}
