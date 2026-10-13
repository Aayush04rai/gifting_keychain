#pragma once

#include <stdbool.h>

// Builds the first watch face on the active LVGL screen. Must be called
// with the board display lock held (board_lock()).
void ui_create(void);

// Switch to the next/previous watch face, wrapping around. Called from
// main/ui/gesture.c in response to a tilt or button click; safe to call from
// within the display's own update loop (no board_lock() needed there).
void ui_next_face(void);
void ui_prev_face(void);

// True while the companion (the character face, always first) is showing.
// gesture.c uses it to keep tilt for the eyes instead of face switching.
bool ui_on_companion_face(void);
