#pragma once

#include <stddef.h>

#include "lvgl.h"

// Every watch face shares one visual frame (per the reference layout): a
// single bordered, rounded card filling most of the screen, holding the
// dial/display plus any secondary info inside it - no separate floating
// complication badges scattered around the screen.
lv_obj_t *watchface_build_frame(lv_obj_t *parent);

// A rounded sub-panel inside a frame for the dial/hero content (e.g. the
// analog scale), anchored to the frame's left edge, `size` square.
lv_obj_t *watchface_build_face_box(lv_obj_t *frame, int32_t size);

// A plain vertical info column inside a frame, anchored to the right edge,
// for secondary text (date, battery, ...) - no nested card/border.
lv_obj_t *watchface_build_info_column(lv_obj_t *frame, int32_t width);

// Formats the current battery reading as "NN%", or "--" when no battery is
// present/measurable (board_get_battery_pct() < 0, e.g. the simulator).
void watchface_format_battery(char *buf, size_t buf_size);

// Fills buf with "Good morning,"/"Good afternoon,"/"Good evening," based
// on the current hour.
void watchface_time_greeting(char *buf, size_t buf_size);
