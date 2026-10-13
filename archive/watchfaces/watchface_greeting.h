#pragma once

#include "lvgl.h"

// The personalized welcome face: "Good morning, {name}" + title (from
// onboarding), with the current time as a quiet accent in the corner.
// First face to actually use the name/title onboarding captures. Returns
// its update timer so the caller can delete it when switching faces.
lv_timer_t *watchface_greeting_build(lv_obj_t *parent);
