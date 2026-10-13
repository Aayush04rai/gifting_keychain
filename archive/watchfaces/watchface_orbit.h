#pragma once

#include "lvgl.h"

// Ornamental concentric-ring dial (inspired by orbital/mandala art: a dense
// patterned "sun" with smaller rings orbiting outward) paired with the
// usual digital time/date/battery info column. Purely decorative rings -
// no needles - since the reference art has no functional clock hands of
// its own. Returns its update timer so the caller can delete it when
// switching to another face.
lv_timer_t *watchface_orbit_build(lv_obj_t *parent);
