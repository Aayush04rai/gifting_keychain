#include <stdio.h>
#include <time.h>

#include "board_hal.h"
#include "watchface_common.h"

#define FRAME_BG     0x12181f
#define FRAME_BORDER 0x2a333d
#define BOX_BORDER   0x333e4a

lv_obj_t *watchface_build_frame(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 304, 140); // near-fullscreen on the content area (below the face-position slider)
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_hex(FRAME_BG), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 20, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(FRAME_BORDER), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

lv_obj_t *watchface_build_face_box(lv_obj_t *frame, int32_t size)
{
    lv_obj_t *box = lv_obj_create(frame);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, size, size);
    lv_obj_align(box, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, 16, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(BOX_BORDER), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}

lv_obj_t *watchface_build_info_column(lv_obj_t *frame, int32_t width)
{
    lv_obj_t *info = lv_obj_create(frame);
    lv_obj_remove_style_all(info);
    lv_obj_set_size(info, width, lv_pct(100));
    lv_obj_align(info, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(info, 6, 0);
    lv_obj_clear_flag(info, LV_OBJ_FLAG_SCROLLABLE);
    return info;
}

void watchface_format_battery(char *buf, size_t buf_size)
{
    int batt = board_get_battery_pct();
    if (batt < 0) {
        snprintf(buf, buf_size, "--");
    } else {
        snprintf(buf, buf_size, "%d%%", batt);
    }
}

void watchface_time_greeting(char *buf, size_t buf_size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    int hour = tm_info ? tm_info->tm_hour : 12;
    const char *part = (hour < 12) ? "morning" : (hour < 17) ? "afternoon" : "evening";
    snprintf(buf, buf_size, "Good %s,", part);
}
