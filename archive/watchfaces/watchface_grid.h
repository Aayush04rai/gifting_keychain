#pragma once

#include "lvgl.h"

// Apple Watch "Infograph"-style dashboard: a 2x2 grid of small tiles
// (time, date, battery, uptime) instead of one hero dial. Returns its
// update timer so the caller can delete it when switching to another face.
lv_timer_t *watchface_grid_build(lv_obj_t *parent);
