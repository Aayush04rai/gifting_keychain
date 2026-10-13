#pragma once

#include "lvgl.h"

#include "params.h"
#include "theme.h"

// Optional mouth. Hidden for most emotions - the eyes carry the character,
// the mouth only appears when it adds something (a smile, a gasp).
void mouth_build(lv_obj_t *root, const char_theme_t *theme);
void mouth_apply(const char_params_t *p, float dy, const char_layout_t *lay);
