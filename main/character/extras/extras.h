#pragma once

#include "lvgl.h"

#include "params.h"
#include "theme.h"

// Small floating marks that sit beside the face: "?" when curious, "z" when
// sleepy, "..." while thinking. Kept as text so they cost almost nothing and
// stay in the muted theme colour - they punctuate, they never shout.
void extras_build(lv_obj_t *root, const char_theme_t *theme);

// The quiet "focused." line under the eyes while focus mode is on. 0 hides
// it, 1 shows it; the engine eases the value so it fades rather than pops.
void extras_set_focus(float alpha);
void extras_apply(const char_params_t *p, uint32_t now_ms, const char_layout_t *lay);
