#pragma once

#include "lvgl.h"

// Fun/pop-culture face styled after the Ben 10 Omnitrix dial (built from
// plain LVGL shapes, no imported artwork/assets) - a bezel with 4 accent
// lights around a green disc with a black hourglass/"X" cut across it,
// plus the time below. NOTE: the Omnitrix is a trademarked Cartoon
// Network/Ben 10 design; this face was added at the requester's explicit
// direction, who is responsible for clearing that IP question before this
// ships on any gifted device. Returns its update timer so the caller can
// delete it when switching to another face.
lv_timer_t *watchface_omnitrix_build(lv_obj_t *parent);
