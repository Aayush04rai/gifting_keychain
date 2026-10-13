#pragma once

#include "lvgl.h"

// "Railway dial" style: a 12-hour numeral ring with a dimmer 24-hour ring
// just inside it (inspired by classic dual-scale office clocks), digital
// time in the hub, date + battery in the usual info column. Returns its
// update timer so the caller can delete it when switching to another face.
lv_timer_t *watchface_dualtime_build(lv_obj_t *parent);
