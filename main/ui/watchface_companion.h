#pragma once

#include "lvgl.h"

// The companion: the Character Engine (main/character/) as a face. It is the
// product's home screen, so ui.c puts it first. Returns the engine's update
// timer so the caller can delete it when switching to another face.
lv_timer_t *watchface_companion_build(lv_obj_t *parent);
