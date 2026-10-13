#pragma once

#include "lvgl.h"

// Bold digital face (Apple Watch "Utility"/classic digital-watch
// inspired): large HH:MM, small running seconds, day/date line, battery
// complication. Returns its update timer so the caller can delete it when
// switching to another face.
lv_timer_t *watchface_digital_build(lv_obj_t *parent);
