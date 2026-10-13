#include <stdio.h>
#include <time.h>

#include "lvgl.h"

#include "board_hal.h"
#include "watchface_common.h"
#include "watchface_grid.h"

#define FACE_BOX_SIZE 120
#define FACE_ACCENT   0xd4af6a
#define DOT_SIZE      36
#define DOT_GAP       8

static lv_obj_t *s_time_label;
static lv_obj_t *s_date_label;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_uptime_label;

static void add_dot(lv_obj_t *box, int32_t x, int32_t y, uint32_t color)
{
    lv_obj_t *dot = lv_obj_create(box);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, DOT_SIZE, DOT_SIZE);
    lv_obj_set_style_radius(dot, 8, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_align(dot, LV_ALIGN_CENTER, x, y);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
}

static void update_grid(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        char time_buf[8];
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
        lv_label_set_text(s_time_label, time_buf);

        char date_buf[16];
        strftime(date_buf, sizeof(date_buf), "%a %d %b", tm_info);
        lv_label_set_text(s_date_label, date_buf);
    }

    char batt_buf[16];
    watchface_format_battery(batt_buf, sizeof(batt_buf));
    char batt_text[24];
    snprintf(batt_text, sizeof(batt_text), LV_SYMBOL_BATTERY_FULL " %s", batt_buf);
    lv_label_set_text(s_battery_label, batt_text);

    uint32_t uptime_s = board_get_uptime_seconds();
    char uptime_buf[24];
    snprintf(uptime_buf, sizeof(uptime_buf), LV_SYMBOL_REFRESH " %02lu:%02lu:%02lu",
             (unsigned long)(uptime_s / 3600), (unsigned long)((uptime_s / 60) % 60),
             (unsigned long)(uptime_s % 60));
    lv_label_set_text(s_uptime_label, uptime_buf);
}

lv_timer_t *watchface_grid_build(lv_obj_t *parent)
{
    lv_obj_t *frame = watchface_build_frame(parent);
    lv_obj_t *box = watchface_build_face_box(frame, FACE_BOX_SIZE);

    // A small nod to this face's original 2x2-tile identity, now living
    // inside the mandated box instead of spanning the whole card; all four
    // metrics that used to be one tile each now stack as text in the info
    // column instead.
    int32_t off = (DOT_SIZE + DOT_GAP) / 2;
    add_dot(box, -off, -off, 0x333e4a);
    add_dot(box, off, -off, FACE_ACCENT);
    add_dot(box, -off, off, FACE_ACCENT);
    add_dot(box, off, off, 0x333e4a);

    lv_obj_t *info = watchface_build_info_column(frame, 140);

    s_time_label = lv_label_create(info);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(FACE_ACCENT), 0);

    s_date_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0xffffff), 0);

    s_battery_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_battery_label, lv_color_hex(0x9fb3c8), 0);

    s_uptime_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_uptime_label, lv_color_hex(0x9fb3c8), 0);

    lv_timer_t *timer = lv_timer_create(update_grid, 1000, NULL);
    update_grid(timer);
    return timer;
}
