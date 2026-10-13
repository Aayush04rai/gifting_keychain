#include <stdio.h>
#include <time.h>

#include "lvgl.h"

#include "watchface_common.h"
#include "watchface_minimal.h"

#define FACE_BOX_SIZE 120

static lv_obj_t *s_time_label;
static lv_obj_t *s_date_label;

static void update_minimal(lv_timer_t *timer)
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
    strftime(date_buf, sizeof(date_buf), "%d %b", tm_info);
    lv_label_set_text(s_date_label, date_buf);
}

lv_timer_t *watchface_minimal_build(lv_obj_t *parent)
{
    lv_obj_t *frame = watchface_build_frame(parent);
    lv_obj_t *box = watchface_build_face_box(frame, FACE_BOX_SIZE);

    // Deliberately the plainest box in the gallery - no ticks, rings or
    // numerals, just the time - keeping this face's "minimal" identity
    // while still honoring the shared box+info layout.
    s_time_label = lv_label_create(box);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(0xffffff), 0);
    lv_obj_center(s_time_label);

    lv_obj_t *info = watchface_build_info_column(frame, 140);

    s_date_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0x63707d), 0);

    lv_timer_t *timer = lv_timer_create(update_minimal, 1000, NULL);
    update_minimal(timer);
    return timer;
}
