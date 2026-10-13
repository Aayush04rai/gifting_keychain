#pragma once

#include "lvgl.h"

// Case-border numerals (inspired by Apple Watch's numeral-on-the-bezel
// styles): the frame's own rounded-rect border carries the 12/3/6/9
// numerals at the compass points, with a bold digital time centered inside.
// Returns its update timer so the caller can delete it when switching to
// another face.
lv_timer_t *watchface_track_build(lv_obj_t *parent);
