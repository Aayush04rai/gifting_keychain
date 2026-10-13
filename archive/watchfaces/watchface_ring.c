#include <stdio.h>
#include <time.h>

#include "lvgl.h"

#include "watchface_common.h"
#include "watchface_ring.h"

#define FACE_BOX_SIZE 132
#define RING_DIAMETER 116
#define FACE_ACCENT   0xd4af6a

static lv_obj_t *s_arc;
static lv_obj_t *s_time_label;
static lv_obj_t *s_date_label;
static lv_obj_t *s_battery_label;

static void update_ring(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (!tm_info) {
        return;
    }

    lv_arc_set_value(s_arc, tm_info->tm_sec);

    char time_buf[8];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
    lv_label_set_text(s_time_label, time_buf);

    char date_buf[16];
    strftime(date_buf, sizeof(date_buf), "%d %b", tm_info);
    lv_label_set_text(s_date_label, date_buf);

    char batt_buf[16];
    watchface_format_battery(batt_buf, sizeof(batt_buf));
    char batt_text[24];
    snprintf(batt_text, sizeof(batt_text), LV_SYMBOL_BATTERY_FULL " %s", batt_buf);
    lv_label_set_text(s_battery_label, batt_text);
}

lv_timer_t *watchface_ring_build(lv_obj_t *parent)
{
    lv_obj_t *frame = watchface_build_frame(parent);
    lv_obj_t *box = watchface_build_face_box(frame, FACE_BOX_SIZE);

    lv_obj_t *arc = lv_arc_create(box);
    lv_obj_set_size(arc, RING_DIAMETER, RING_DIAMETER);
    lv_obj_center(arc);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_range(arc, 0, 60);
    lv_arc_set_value(arc, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x262f39), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(FACE_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    s_arc = arc;

    s_time_label = lv_label_create(arc);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(0xffffff), 0);
    lv_obj_align(s_time_label, LV_ALIGN_CENTER, 0, -6);

    s_date_label = lv_label_create(arc);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0x9fb3c8), 0);
    lv_obj_align(s_date_label, LV_ALIGN_CENTER, 0, 16);

    lv_obj_t *info = watchface_build_info_column(frame, 140);

    s_battery_label = lv_label_create(info);
    lv_obj_set_style_text_font(s_battery_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_battery_label, lv_color_hex(0xffffff), 0);

    lv_timer_t *timer = lv_timer_create(update_ring, 1000, NULL);
    update_ring(timer);
    return timer;
}
