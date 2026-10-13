cmake_minimum_required(VERSION 3.16)

# Managed component dependencies (LVGL, esp_lvgl_port, led_strip, and the
# waveshare/esp32_c5_lcd_1_47 BSP) are declared in main/idf_component.yml and
# are fetched automatically by the Component Manager on first build.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(c5_lvgl_ui)
