#pragma once

#include "lvgl.h"

// Modern "chronograph" face: a centered ring that sweeps once per minute
// (seconds progress), digital HH:MM and date inside the ring, battery
// below. Returns its update timer so the caller can delete it when
// switching to another face.
lv_timer_t *watchface_ring_build(lv_obj_t *parent);
