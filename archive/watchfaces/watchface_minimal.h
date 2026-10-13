#pragma once

#include "lvgl.h"

// The "do one thing perfectly" face: huge centered time, a quiet date line
// underneath, nothing else - no frame, no complications. Returns its
// update timer so the caller can delete it when switching to another face.
lv_timer_t *watchface_minimal_build(lv_obj_t *parent);
