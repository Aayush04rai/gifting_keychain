cmake_minimum_required(VERSION 3.16)
project(c5_lvgl_ui_sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# All config lives under ../config: lv_conf_sim.h is this build's LVGL
# config (board_config.h and sim_config.h are picked up via relative
# #includes from the .c/.h files that need them).
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../config/lv_conf_sim.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

find_package(SDL2 CONFIG REQUIRED)

# Reuse the LVGL sources the ESP-IDF build already fetched via the
# Component Manager instead of pulling a second copy.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/lvgl__lvgl ${CMAKE_CURRENT_BINARY_DIR}/lvgl)

# The simulator's "SD card": <project>/assets/sd/, read through LVGL's stdio file-system driver as drive S:.
get_filename_component(SIM_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/sd/" ABSOLUTE)
target_compile_definitions(lvgl PUBLIC SIM_ASSET_ROOT="${SIM_ASSET_DIR}/")

# LVGL's own SDL driver sources (src/drivers/sdl/*) need SDL2's headers to
# compile, so the lvgl library target itself - not just our executable -
# must see them.
target_link_libraries(lvgl PUBLIC SDL2::SDL2)

add_executable(c5_lvgl_ui_sim
    main_sim.c
    board_sim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/character/character.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/character/expressions.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/character/acts/acts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/character/eyes/eyes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/character/mouth/mouth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/character/extras/extras.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/character/status/status.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/character/led/led.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/ui/watchface_companion.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/ui/ui.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/ui/gesture.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/onboarding/onboarding_sim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/personalization/news_sim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/personalization/jokes_sim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/personalization/jokes_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/personalization/text_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/personalization/wellness.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/events/events.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/character/card/card.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/power/power_sim.c
)

# main/board gives access to board_hal.h (shared with the real firmware),
# main/ui to ui.h/gesture.h/watchface_*.h, main/onboarding to onboarding.h;
# main_sim.c and board_sim.c both include board_hal.h.
target_compile_definitions(c5_lvgl_ui_sim PRIVATE CHARACTER_DEBUG_ENV EVENTS_DEBUG_ENV)

target_include_directories(c5_lvgl_ui_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/board
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/character
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/events
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/power
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/onboarding
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/personalization
)

target_link_libraries(c5_lvgl_ui_sim PRIVATE
    lvgl::lvgl
    SDL2::SDL2
)
