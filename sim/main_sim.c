// Keep a plain int main() instead of letting SDL2main.lib provide WinMain -
// simplest option for a console-based CMake build on Windows.
#define SDL_MAIN_HANDLED
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_window.h"

#include "board_hal.h"
#include "events.h"
#include "gesture.h"
#include "jokes.h"
#include "news.h"
#include "ui.h"

#include "../config/sim_config.h"

static uint32_t sdl_tick_get(void)
{
    return SDL_GetTicks();
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    SDL_SetMainReady();
    board_init();
    lv_init();
    lv_tick_set_cb(sdl_tick_get);

    lv_display_t *disp = lv_sdl_window_create(BOARD_LCD_H_RES, BOARD_LCD_V_RES);
    lv_sdl_window_set_title(disp, SIM_WINDOW_TITLE);
    lv_sdl_window_set_zoom(disp, SIM_WINDOW_ZOOM);

    lv_sdl_mouse_create();

    news_start();
    jokes_start();
    ui_create();
    events_start();
    gesture_start();

    // SIM_START_FACE lets test tooling jump straight to a given face index
    // on launch, without depending on the arrow-key gesture path (Windows
    // input-injection into a background SDL window is unreliable).
    const char *start_face_env = getenv("SIM_START_FACE");
    if (start_face_env) {
        int start_face = atoi(start_face_env);
        for (int i = 0; i < start_face; i++) {
            ui_next_face();
        }
    }

    // lv_sdl_window_create() already pumps SDL's event queue (including
    // SDL_QUIT, which it handles by exiting since LV_SDL_DIRECT_EXIT is
    // set), so this loop only needs to drive LVGL's own timers.
    while (1) {
        uint32_t delay_ms = lv_timer_handler();
        SDL_Delay(delay_ms ? delay_ms : 5);
    }

    return 0;
}
