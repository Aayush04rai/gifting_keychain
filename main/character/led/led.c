#include <math.h>

#include "board_hal.h"
#include "led.h"

#define TAU 6.2831853f

typedef struct {
    uint8_t r, g, b;   // colour at full level
    float level;       // steady brightness, 0..1
    float pulse;       // extra brightness swing, 0..1 (0 = steady)
    uint32_t period_ms; // pulse period
} led_look_t;

// Colours stay inside the product palette: cool steel for ambient, the gold
// accent for "something happened", white only for surprise.
//
// The scale is the LED's full range: a channel's value is simply
// colour * level (so level 1.0 on white is 255/255/255). Ambient states stay
// low so the device never glows by itself; the expressive moments - a flash,
// a pulse, waking up - are allowed to go bright, which is what makes them
// read as part of the character.
static const led_look_t k_look[CHAR_EMOTION_COUNT] = {
    [CHAR_EMOTION_IDLE]      = {120, 150, 190, 0.18f, 0.08f, 8000},  // quiet ambient, slow drift
    [CHAR_EMOTION_CURIOUS]   = {212, 175, 106, 0.70f, 0.00f, 0},
    [CHAR_EMOTION_HAPPY]     = {212, 175, 106, 0.60f, 0.35f, 900},
    [CHAR_EMOTION_SURPRISED] = {255, 255, 255, 1.00f, 0.00f, 0},
    [CHAR_EMOTION_THINKING]  = {90, 140, 255, 0.35f, 0.60f, 2400},   // slow pulse
    [CHAR_EMOTION_SLEEPY]    = {120, 150, 190, 0.08f, 0.00f, 0},
    [CHAR_EMOTION_SLEEPING]  = {0, 0, 0, 0.00f, 0.00f, 0},           // fades out
    [CHAR_EMOTION_DIZZY]     = {212, 175, 106, 0.55f, 0.45f, 380},   // quick flicker
    [CHAR_EMOTION_WAKING1]   = {120, 150, 190, 0.14f, 0.00f, 0},
    [CHAR_EMOTION_WAKING2]   = {120, 150, 190, 0.30f, 0.00f, 0},
    [CHAR_EMOTION_SMIRK]     = {212, 175, 106, 0.50f, 0.50f, 500},   // quick playful flash
    [CHAR_EMOTION_CALM]      = {110, 170, 200, 0.22f, 0.20f, 6000},  // soft, slow
    [CHAR_EMOTION_BREATHE]   = {110, 170, 200, 0.12f, 0.80f, 5000},  // matches the 5 s breath
    [CHAR_EMOTION_EXHAUSTED] = {255, 30, 20, 0.20f, 0.80f, 1400},    // low battery: a red, labouring pulse
    // Same gold as every other expressive state - the lighting does not change
    // with the companion - just an impatient, quicker flicker.
    [CHAR_EMOTION_ANGRY]     = {212, 175, 106, 0.50f, 0.50f, 420},
    [CHAR_EMOTION_EFFORT]    = {212, 175, 106, 0.45f, 0.45f, 800},   // a steady working pulse
    [CHAR_EMOTION_SAD]       = {110, 140, 200, 0.18f, 0.20f, 5000},  // cool and low
};

// Full 8-bit range per channel. (An earlier version capped this at 60/255,
// which is why even the brightest moments looked dim.)
#define LED_MAX 255.0f

static bool s_quiet;
static float s_r, s_g, s_b;       // eased output, in 0..LED_MAX
static uint8_t s_out_r, s_out_g, s_out_b;
static bool s_started;

void led_apply(char_emotion_t emotion, uint32_t now_ms, float boost)
{
    const led_look_t *l = &k_look[emotion];

    float level = l->level;
    if (l->period_ms) {
        level += l->pulse * 0.5f * (1.0f + sinf((float)now_ms * (TAU / (float)l->period_ms)));
    }
    level += boost;
    if (level > 1.0f) {
        level = 1.0f;
    }
    if (s_quiet) {
        level *= 0.25f;
    }

    // Ease toward the target; the wake-up boost and dizzy flicker still read
    // clearly at this rate, and sleep fades out over about a second.
    const float k = 0.12f;
    s_r += (l->r / 255.0f * level * LED_MAX - s_r) * k;
    s_g += (l->g / 255.0f * level * LED_MAX - s_g) * k;
    s_b += (l->b / 255.0f * level * LED_MAX - s_b) * k;

    uint8_t r = (uint8_t)(s_r + 0.5f);
    uint8_t g = (uint8_t)(s_g + 0.5f);
    uint8_t b = (uint8_t)(s_b + 0.5f);

    // Only touch the hardware when the value actually changes.
    if (!s_started || r != s_out_r || g != s_out_g || b != s_out_b) {
        s_started = true;
        s_out_r = r;
        s_out_g = g;
        s_out_b = b;
        board_set_led(r, g, b);
    }
}

void led_set_quiet(bool quiet)
{
    s_quiet = quiet;
}

void led_off(void)
{
    s_r = s_g = s_b = 0.0f;
    s_out_r = s_out_g = s_out_b = 0;
    s_started = true;
    board_set_led(0, 0, 0);
}
