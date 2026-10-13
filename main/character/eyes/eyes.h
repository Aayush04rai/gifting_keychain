#pragma once

#include "lvgl.h"

#include "params.h"
#include "theme.h"

// Both eyes: rounded body, a pupil that tracks the gaze, and top/bottom lids.
// The lids are children of the eye so the eye's rounded corners clip them -
// that is what makes blinks and squints read as eyelids, not rectangles.
//
// With `lashes` set, each eye gets three fine eyelashes sweeping up and out
// from its outer upper corner (the female companion). They are drawn in the
// eye colour, follow the eye as it moves, closes and blinks, and are the only
// visual difference between the two companions' faces.
void eyes_build(lv_obj_t *root, const char_theme_t *theme, bool lashes);

// gaze_x/gaze_y: -1..1 final gaze (idle glances + emotion bias + IMU).
// blink: 0 open .. 1 shut, layered on top of the lids.
// dy: whole-face vertical offset in px (breathing).
void eyes_apply(const char_params_t *p, float gaze_x, float gaze_y, float blink, float dy, const char_layout_t *lay);
