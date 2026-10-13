#include <math.h>

#include "lvgl.h"

#include "board_hal.h"
#include "character.h"
#include "gesture.h"
#include "ui.h"

// Tilt past this (in g, on the axis running along the keychain's width)
// counts as a deliberate "swipe"; it has to fall back under the reset
// threshold before another swipe can fire, so holding it tilted doesn't
// repeat-fire.
#define GESTURE_TILT_TRIGGER_G 0.5f
#define GESTURE_TILT_RESET_G   0.2f
// ...and it only counts if held this many polls with the board calm (the
// change since the previous poll under CALM_G), so a jerk passing through
// the tilt range can't flip the face.
#define GESTURE_TILT_HOLD_POLLS 2
#define GESTURE_TILT_CALM_G    0.35f

#define GESTURE_POLL_PERIOD_MS 150
#define GESTURE_POLL_DEEP_MS   400

// A jerk is sudden change in the acceleration vector, not a sustained tilt:
// tipping the keychain slowly moves it a few tenths of a g per poll, a real
// jerk moves it 1-4 g. Either one poll above DELTA_G, or the last
// SHAKE_WINDOW polls adding up past WINDOW_G, counts. After one, ignore
// everything for a moment so the jerk itself can't also flip the face.
#define GESTURE_SHAKE_DELTA_G  1.0f
#define GESTURE_SHAKE_WINDOW_G 2.0f
// A single change this hard counts with no reversal needed; a reversal is two
// consecutive changes in opposite directions, each at least REVERSAL_MIN_G.
#define GESTURE_SHAKE_HARD_G   2.4f
#define GESTURE_REVERSAL_MIN_G 0.4f
#define SHAKE_WINDOW           3
#define GESTURE_SHAKE_LOCKOUT_MS 1200

// Tilt steers the character's pupils. It is measured against a slowly
// moving baseline (time constant ~4 s) rather than against "flat", so a
// keychain hanging from a bag or lying on its side doesn't stare sideways
// forever: it looks where you tip it, then settles back. GAZE_FULL_G is how
// far past the baseline gives a full-range look. Flip a sign if the pupils
// move the wrong way on the real board.
#define GAZE_FULL_G       0.30f
#define GAZE_DEADZONE     0.15f
#define GAZE_BASELINE_K   0.04f
#define GAZE_HOLD_MS      350

// Face-down for focus mode (see below): z at or under FLIP_DOWN_Z, lying
// still (poll-to-poll change under FLIP_CALM_G), for FLIP_HOLD_POLLS polls
// (about 2.1 s); face-up again is z over FLIP_UP_Z for FLIP_UP_POLLS polls.
#define FLIP_DOWN_Z      (-0.8f)
#define FLIP_UP_Z        0.5f
#define FLIP_CALM_G      0.25f
#define FLIP_HOLD_POLLS  14
#define FLIP_UP_POLLS    3

// Orientation of the sensor relative to the screen (see the swap below).
// Flip a sign if tipping the board moves the pupils the wrong way.
#define IMU_LATERAL_SIGN  (+1.0f)
#define IMU_VERTICAL_SIGN (+1.0f)

static bool s_armed = true;
static bool s_have_prev;
static float s_prev_x, s_prev_y, s_prev_z;
static float s_base_lat, s_base_vert;
static uint32_t s_lockout_until;
static float s_delta_hist[SHAKE_WINDOW];
static unsigned s_delta_idx;
static int s_tilt_polls;
static float s_prev_dx, s_prev_dy, s_prev_dz, s_prev_delta;
static int s_reversal_polls;
static int s_down_polls;
static int s_up_polls;
static bool s_poll_deep;  // the poll timer is currently on its slow deep-sleep period
static bool s_flip_focus; // focus was started by lying face-down

// Runs inside the display's own update loop (like each watch face's own
// 1s timer), so it can call ui_next_face()/ui_prev_face() directly without
// board_lock().
static void gesture_poll_cb(lv_timer_t *timer)
{
    // In deep sleep the screen is off and the chip is light-sleeping between
    // timers, so poll the IMU only a few times a second (just enough to notice
    // a pick-up quickly) instead of ~7: every poll is a wake-up.
    bool deep = character_is_deep();
    if (deep != s_poll_deep) {
        s_poll_deep = deep;
        lv_timer_set_period(timer, deep ? GESTURE_POLL_DEEP_MS : GESTURE_POLL_PERIOD_MS);
    }

    bool clicked = board_button_clicked();
    bool long_press = board_button_long_pressed();

    float x, y, z;
    board_get_accel(&x, &y, &z);

    float dx = x - s_prev_x, dy = y - s_prev_y, dz = z - s_prev_z;
    float delta = s_have_prev ? sqrtf(dx * dx + dy * dy + dz * dz) : 0.0f;

    // What separates a shake from deliberately turning the device over (the
    // face-down gesture) or tipping it: a shake goes back and forth. The
    // acceleration jumps one way and then the other, so two consecutive
    // changes point in opposite directions; a flip or a tilt is one smooth
    // move in a single direction and never reverses. So:
    //   - a very hard single change always counts (a real knock or drop);
    //   - otherwise a big change, or several moderate ones in a row, only
    //     counts if there has been a reversal within the last few polls.
    bool reversal = s_have_prev && s_prev_delta > GESTURE_REVERSAL_MIN_G && delta > GESTURE_REVERSAL_MIN_G &&
                    (dx * s_prev_dx + dy * s_prev_dy + dz * s_prev_dz) < 0.0f;
    if (reversal) {
        s_reversal_polls = SHAKE_WINDOW;
    } else if (s_reversal_polls > 0) {
        s_reversal_polls--;
    }
    s_prev_dx = dx;
    s_prev_dy = dy;
    s_prev_dz = dz;
    s_prev_delta = delta;

    s_delta_hist[s_delta_idx++ % SHAKE_WINDOW] = delta;
    float recent = 0.0f;
    for (int i = 0; i < SHAKE_WINDOW; i++) {
        recent += s_delta_hist[i];
    }
    bool shaken = delta > GESTURE_SHAKE_HARD_G ||
                  (s_reversal_polls > 0 && (delta > GESTURE_SHAKE_DELTA_G || recent > GESTURE_SHAKE_WINDOW_G));
    if (shaken) {
        for (int i = 0; i < SHAKE_WINDOW; i++) {
            s_delta_hist[i] = 0.0f;
        }
        s_reversal_polls = 0;
    }
    // The IMU is mounted for the panel's native portrait orientation but the
    // UI runs in landscape, so the sensor's axes are swapped relative to the
    // screen: tipping the screen's left/right edge moves the sensor's Y,
    // tipping its top/bottom edge moves the sensor's X.
    float lat = y * IMU_LATERAL_SIGN;  // + = right edge down
    float vert = x * IMU_VERTICAL_SIGN; // + = bottom edge down (look down)

    if (!s_have_prev) {
        s_base_lat = lat;
        s_base_vert = vert;
    }
    s_prev_x = x;
    s_prev_y = y;
    s_prev_z = z;
    s_have_prev = true;

    // Movement of any kind keeps the character awake; a button press counts
    // as the strongest kind.
    character_motion((clicked || long_press) ? 1.0f : delta);

    // Face-down = focus (do not disturb), like silencing a phone: lay the
    // device screen-down and leave it, and after about two seconds focus mode
    // starts; turn it face-up again and it ends. Face-down is z near -1 g (it
    // reads about +1 g face-up). It must be lying still to count, so a device
    // swinging past face-down on a lanyard does not trigger it.
    if (z < FLIP_DOWN_Z && delta < FLIP_CALM_G) {
        s_down_polls++;
        s_up_polls = 0;
    } else if (z > FLIP_UP_Z) {
        s_up_polls++;
        s_down_polls = 0;
    } else {
        s_down_polls = 0; // on its side or being carried: neither
    }
    if (!s_flip_focus && s_down_polls >= FLIP_HOLD_POLLS) {
        s_flip_focus = true;
        character_set_focus(true);
    }
    if (s_flip_focus && s_up_polls >= FLIP_UP_POLLS) {
        s_flip_focus = false;
        character_set_focus(false);
    }

    float gaze_x = (lat - s_base_lat) / GAZE_FULL_G;
    float gaze_y = (vert - s_base_vert) / GAZE_FULL_G;
    // The resting position normally drifts toward wherever the board is held
    // (so a hanging keychain doesn't stare sideways). While a check-in is
    // being answered it must hold still, or a tilt that is being held for the
    // answer would fade away underneath the person.
    if (!character_input_active()) {
        s_base_lat += (lat - s_base_lat) * GAZE_BASELINE_K;
        s_base_vert += (vert - s_base_vert) * GAZE_BASELINE_K;
    }
    if (!shaken && !character_input_active() && (fabsf(gaze_x) > GAZE_DEADZONE || fabsf(gaze_y) > GAZE_DEADZONE)) {
        character_look(fmaxf(-1.0f, fminf(1.0f, gaze_x)), fmaxf(-1.0f, fminf(1.0f, gaze_y)), GAZE_HOLD_MS);
    }

    uint32_t now = lv_tick_get();
    if (shaken) {
        character_on_shake();
        s_lockout_until = now + GESTURE_SHAKE_LOCKOUT_MS;
        s_armed = false;
        return;
    }
    if (now < s_lockout_until) {
        return;
    }

    // A check-in is waiting for an answer: the tilt goes to it (the engine
    // decides what is selected and for how long it has been held) and nothing
    // else - no face change, no focus toggle - happens meanwhile. The device
    // has no buttons to rely on, so tilt is the whole interface.
    if (character_input_active()) {
        character_input_tilt(gaze_x);
        return;
    }

    if (long_press) {
        // Focus mode lives on the companion; on other faces a long press is ignored.
        if (ui_on_companion_face()) {
            character_toggle_focus();
        }
    } else if (clicked) {
        ui_next_face();
    }

    // On the companion, tilt belongs to the eyes (and the BOOT button changes
    // face), so a tilt never flips it away. Everywhere else a deliberate tilt
    // does: past the trigger, held for two polls, and not still in motion.
    if (ui_on_companion_face()) {
        s_tilt_polls = 0;
        s_armed = false;
        return;
    }

    bool calm = delta < GESTURE_TILT_CALM_G;
    if (fabsf(lat) > GESTURE_TILT_TRIGGER_G && calm) {
        s_tilt_polls++;
    } else {
        s_tilt_polls = 0;
    }

    if (s_armed) {
        if (s_tilt_polls >= GESTURE_TILT_HOLD_POLLS) {
            if (lat > 0) {
                ui_next_face();
            } else {
                ui_prev_face();
            }
            s_armed = false;
            s_tilt_polls = 0;
        }
    } else if (fabsf(lat) < GESTURE_TILT_RESET_G) {
        s_armed = true;
    }
}

void gesture_start(void)
{
    lv_timer_create(gesture_poll_cb, GESTURE_POLL_PERIOD_MS, NULL);
}
