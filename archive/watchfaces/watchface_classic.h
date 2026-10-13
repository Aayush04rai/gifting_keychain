#pragma once

#include "lvgl.h"

// Apple Watch "Simple"-inspired analog dial: minimal ticks (no numerals),
// hour/minute/second hands, date + battery complications. Returns its
// update timer so the caller can delete it when switching to another face.
lv_timer_t *watchface_classic_build(lv_obj_t *parent);
