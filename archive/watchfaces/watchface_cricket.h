#pragma once

#include "lvgl.h"

// Live cricket score face - only added to the gallery (see ui.c) when the
// recipient checked "cricket" during onboarding. Box: the two teams'
// short codes with "vs" between them. Info: each team's score/overs plus
// the match status text. Returns its update timer so the caller can
// delete it when switching to another face.
lv_timer_t *watchface_cricket_build(lv_obj_t *parent);
