#include <stdio.h>
#include <time.h>

#include "lvgl.h"

#include "watchface_classic.h"
#include "watchface_common.h"

#define FACE_BOX_SIZE 120
#define DIAL_SIZE     106
#define FACE_ACCENT   0xd4af6a // warm gold - the recurring accent color across all faces

static lv_obj_t *s_scale;
static lv_obj_t *s_hour_needle;
static lv_obj_t *s_minute_needle;
static lv_obj_t *s_second_needle;
static lv_obj_t *s_date_label;
static lv_obj_t *s_battery_label;

static void style_needle(lv_obj_t *line, uint32_t color, int32_t width)
{
    lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
}

static void update_classic(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    // Plain localtime() (not _r/_s) is the one call that exists unchanged
    // on both newlib (ESP-IDF) and MSVC (the simulator); safe here since
    // it's only ever called from this single LVGL timer.
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (!tm_info) {
        return;
    }

    int hour12 = tm_info->tm_hour % 12;
    int32_t hour_val = hour12 * 5 + tm_info->tm_min / 12;
    int32_t minute_val = tm_info->tm_min;
    int32_t second_val = tm_info->tm_sec;

    lv_scale_set_line_needle_value(s_scale, s_hour_needle, -42, hour_val);
    lv_scale_set_line_needle_value(s_scale, s_minute_needle, -18, minute_val);
    lv_scale_set_line_needle_value(s_scale, s_second_needle, -12, second_val);

    char date_buf[16];
    strftime(date_buf, sizeof(date_buf), "%a %d", tm_info);
    lv_label_set_text(s_date_label, date_buf);

    char batt_buf[16];
    watchface_format_battery(batt_buf, sizeof(batt_buf));
    char batt_text[24];
    snprintf(batt_text, sizeof(batt_text), LV_SYMBOL_BATTERY_FULL " %s", batt_buf);
    lv_label_set_text(s_battery_label, batt_text);
}

lv_timer_t *watchface_classic_build(lv_obj_t *parent)
{
    lv_obj_t *frame = watchface_build_frame(parent);
    lv_obj_t *box = watchface_build_face_box(frame, FACE_BOX_SIZE);

    lv_obj_t *scale = lv_scale_create(box);
    lv_obj_set_size(scale, DIAL_SIZE, DIAL_SIZE);
    lv_obj_center(scale);
    lv_obj_set_style_bg_opa(scale, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_scale_set_mode(scale, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(scale, 0, 60);
    lv_scale_set_total_tick_count(scale, 60);
    lv_scale_set_major_tick_every(scale, 5);
    lv_scale_set_angle_range(scale, 360);
    lv_scale_set_rotation(scale, 270); // 0 value = 12 o'clock
    lv_scale_set_label_show(scale, false);
    lv_obj_set_style_length(scale, 3, LV_PART_ITEMS);
    lv_obj_set_style_line_color(scale, lv_color_hex(0x3a4550), LV_PART_ITEMS);
    lv_obj_set_style_line_width(scale, 1, LV_PART_ITEMS);
    lv_obj_set_style_length(scale, 7, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(scale, lv_color_hex(0xd8dee6), LV_PART_INDICATOR);
    lv_obj_set_style_line_width(scale, 2, LV_PART_INDICATOR);
    s_scale = scale;

    s_hour_needle = lv_line_create(scale);
    style_needle(s_hour_needle, 0xffffff, 4);

    s_minute_needle = lv_line_create(scale);
    style_needle(s_minute_needle, 0xffffff, 3);

    s_second_needle = lv_line_create(scale);
    style_needle(s_second_needle, FACE_ACCENT, 1);

    lv_obj_t *center_dot = lv_obj_create(scale);
    lv_obj_remove_style_all(center_dot);
    lv_obj_set_size(center_dot, 7, 7);
    lv_obj_set_style_bg_color(center_dot, lv_color_hex(FACE_ACCENT), 0);
    lv_obj_set_style_bg_opa(center_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_center(center_dot);

    lv_obj_t *info = watchface_build_info_column(frame, 140);

    s_date_label = lv_label_create(info);
    lv_obj_set_style_text_font(s_date_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0xffffff), 0);

    s_battery_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_battery_label, lv_color_hex(0x9fb3c8), 0);

    lv_timer_t *timer = lv_timer_create(update_classic, 1000, NULL);
    update_classic(timer);
    return timer;
}
