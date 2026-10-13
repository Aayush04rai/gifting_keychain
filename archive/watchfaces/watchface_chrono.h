#pragma once

#include "lvgl.h"

// Technical "sport chronograph" style: analog hands in a blue-accented dial
// plus two small sub-dials (day-of-week, battery) - inspired by multi-
// complication chronograph faces, without any of the branding/weather-data
// specifics of the reference (we have no weather source on this board, so
// a battery sub-dial stands in instead). Returns its update timer so the
// caller can delete it when switching to another face.
lv_timer_t *watchface_chrono_build(lv_obj_t *parent);
