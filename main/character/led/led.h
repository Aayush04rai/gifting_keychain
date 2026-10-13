#pragma once

#include <stdint.h>

#include "character.h"

// The RGB LED as part of the character rather than a status light: each
// emotion has a colour and a way of breathing, and the output eases between
// them so the light never snaps. Brightness is deliberately low - a keychain
// that glows like a notification LED would stop feeling premium.
//
// boost: 0..1 extra brightness that decays on its own, used for one-off
// moments (the bright pulse when waking up).
void led_apply(char_emotion_t emotion, uint32_t now_ms, float boost);

// Quiet mode (focus): the same colours and rhythms at a fraction of the
// brightness, so the device stays out of the corner of someone's eye.
void led_set_quiet(bool quiet);

// Cuts the light immediately (called when the character leaves the screen).
void led_off(void);
