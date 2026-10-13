#include <stdio.h>
#include <time.h>

#include "lvgl.h"

#include "watchface_common.h"
#include "watchface_digital.h"

#define FACE_BOX_SIZE 120
#define FACE_ACCENT   0xd4af6a

static lv_obj_t *s_time_label;
static lv_obj_t *s_seconds_label;
static lv_obj_t *s_date_label;
static lv_obj_t *s_battery_label;

static void update_digital(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (!tm_info) {
        return;
    }

    char time_buf[8];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
    lv_label_set_text(s_time_label, time_buf);

    char sec_buf[8];
    snprintf(sec_buf, sizeof(sec_buf), ":%02d", tm_info->tm_sec);
    lv_label_set_text(s_seconds_label, sec_buf);

    char date_buf[16];
    strftime(date_buf, sizeof(date_buf), "%a %d %b", tm_info);
    lv_label_set_text(s_date_label, date_buf);

    char batt_buf[16];
    watchface_format_battery(batt_buf, sizeof(batt_buf));
    char batt_text[24];
    snprintf(batt_text, sizeof(batt_text), LV_SYMBOL_BATTERY_FULL " %s", batt_buf);
    lv_label_set_text(s_battery_label, batt_text);
}

lv_timer_t *watchface_digital_build(lv_obj_t *parent)
{
    lv_obj_t *frame = watchface_build_frame(parent);
    lv_obj_t *box = watchface_build_face_box(frame, FACE_BOX_SIZE);

    // Plain digital readout in the box - no dial, no numerals - to keep
    // this face distinct from Classic/Track/Dualtime's more decorated ones.
    lv_obj_t *time_row = lv_obj_create(box);
    lv_obj_remove_style_all(time_row);
    lv_obj_set_size(time_row, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(time_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_clear_flag(time_row, LV_OBJ_FLAG_SCROLLABLE);

    s_time_label = lv_label_create(time_row);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(0xffffff), 0);

    s_seconds_label = lv_label_create(time_row);
    lv_obj_set_style_text_color(s_seconds_label, lv_color_hex(FACE_ACCENT), 0);
    lv_obj_set_style_pad_left(s_seconds_label, 3, 0);

    lv_obj_t *info = watchface_build_info_column(frame, 140);

    s_date_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0xffffff), 0);

    s_battery_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_battery_label, lv_color_hex(0x9fb3c8), 0);

    lv_timer_t *timer = lv_timer_create(update_digital, 1000, NULL);
    update_digital(timer);
    return timer;
}
