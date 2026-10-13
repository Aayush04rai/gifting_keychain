#pragma once

#include "lvgl.h"

#include "theme.h"

// The quiet status strip along the top edge. For now just the clock; battery
// and Wi-Fi arrive with phase 4. It stays small and muted so the character,
// not the data, is what you notice.
void status_build(lv_obj_t *root, const char_theme_t *theme);
void status_apply(uint32_t now_ms);
