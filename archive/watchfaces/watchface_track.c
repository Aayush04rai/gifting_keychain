#include <stdio.h>
#include <time.h>

#include "lvgl.h"

#include "watchface_common.h"
#include "watchface_track.h"

#define FACE_BOX_SIZE 120
#define NUMERAL_CLR   0xd8dee6

static lv_obj_t *s_time_label;
static lv_obj_t *s_date_label;
static lv_obj_t *s_battery_label;

static void add_numeral(lv_obj_t *box, const char *text, lv_align_t align, int32_t x, int32_t y)
{
    lv_obj_t *lbl = lv_label_create(box);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(NUMERAL_CLR), 0);
    lv_obj_align(lbl, align, x, y);
}

static void update_track(lv_timer_t *timer)
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

    char date_buf[16];
    strftime(date_buf, sizeof(date_buf), "%a %d", tm_info);
    lv_label_set_text(s_date_label, date_buf);

    char batt_buf[16];
    watchface_format_battery(batt_buf, sizeof(batt_buf));
    char batt_text[24];
    snprintf(batt_text, sizeof(batt_text), LV_SYMBOL_BATTERY_FULL " %s", batt_buf);
    lv_label_set_text(s_battery_label, batt_text);
}

lv_timer_t *watchface_track_build(lv_obj_t *parent)
{
    lv_obj_t *frame = watchface_build_frame(parent);
    lv_obj_t *box = watchface_build_face_box(frame, FACE_BOX_SIZE);

    // The case-border numerals ring the box itself instead of the whole
    // card, matching the mandated box+info layout.
    add_numeral(box, "12", LV_ALIGN_TOP_MID, 0, 6);
    add_numeral(box, "3", LV_ALIGN_RIGHT_MID, -8, 0);
    add_numeral(box, "6", LV_ALIGN_BOTTOM_MID, 0, -6);
    add_numeral(box, "9", LV_ALIGN_LEFT_MID, 8, 0);

    s_time_label = lv_label_create(box);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(0xffffff), 0);
    lv_obj_center(s_time_label);

    lv_obj_t *info = watchface_build_info_column(frame, 140);

    s_date_label = lv_label_create(info);
    lv_obj_set_style_text_font(s_date_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0xffffff), 0);

    s_battery_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_battery_label, lv_color_hex(0x9fb3c8), 0);

    lv_timer_t *timer = lv_timer_create(update_track, 1000, NULL);
    update_track(timer);
    return timer;
}
