#pragma once

// Haptic patterns: sequences of buzzes and rests played on the haptic motor
// (board_haptic_pulse), used for "find my keychain" and, later, notifications.
// A pattern is a list of steps {buzz ms, strength %, rest ms}; when it reaches the
// end it jumps back to its `loop_from` step (so a ring can start gently and settle
// into something firmer), or, with loop_from < 0, simply ends.
//
// Firmware only.

#include <stdbool.h>

typedef enum {
    HAPTIC_PAT_DOUBLE_TAP = 0, // two taps, a pause: unobtrusive
    HAPTIC_PAT_HEARTBEAT,      // short-long, like a pulse
    HAPTIC_PAT_SONAR,          // three quick pings, a long pause
    HAPTIC_PAT_STRONGEST,      // builds to continuous firm buzzes within ~3 s: the "find me"
    HAPTIC_PAT_URGENT,         // rapid bursts
    HAPTIC_PAT_TICK,           // one soft tick, once: an option highlighted / chosen
    HAPTIC_PAT_NOTIFY,         // a soft double tap, once: a notification
    HAPTIC_PAT_COUNT,
} haptic_pattern_id_t;

// Starts a pattern (replacing any playing). Returns immediately.
void haptic_pattern_start(haptic_pattern_id_t id);
// Stops it and the motor.
void haptic_pattern_stop(void);
const char *haptic_pattern_name(haptic_pattern_id_t id);
// True while a repeating pattern (a ring) is playing; one-shots do not count.
bool haptic_pattern_looping(void);
