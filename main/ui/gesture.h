#pragma once

// Turns board_get_accel()/board_button_clicked() (board_hal.h) into UI
// navigation. This board has no touch panel, so tilting it left/right (or
// a BOOT button click) is how the recipient moves between screens.
// Runs its own background poll job; call once after ui_create().
void gesture_start(void);
