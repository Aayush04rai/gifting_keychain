#include <stdio.h>
#include <time.h>

#include "lvgl.h"

#include "onboarding.h"
#include "watchface_common.h"
#include "watchface_greeting.h"

#define FACE_BOX_SIZE 120
#define FACE_ACCENT   0xd4af6a

static lv_obj_t *s_time_label;
static lv_obj_t *s_greeting_label;

static void update_greeting(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    char greeting[24];
    watchface_time_greeting(greeting, sizeof(greeting));
    lv_label_set_text(s_greeting_label, greeting);

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        char time_buf[8];
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
        lv_label_set_text(s_time_label, time_buf);
    }
}

lv_timer_t *watchface_greeting_build(lv_obj_t *parent)
{
    lv_obj_t *frame = watchface_build_frame(parent);
    lv_obj_t *box = watchface_build_face_box(frame, FACE_BOX_SIZE);

    // The "watch face" box carries the time; the personalized greeting -
    // this face's whole reason for existing - lives in the info column.
    s_time_label = lv_label_create(box);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(FACE_ACCENT), 0);
    lv_obj_center(s_time_label);

    lv_obj_t *info = watchface_build_info_column(frame, 140);

    s_greeting_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_greeting_label, lv_color_hex(0x9fb3c8), 0);

    lv_obj_t *name_label = lv_label_create(info);
    lv_label_set_text(name_label, onboarding_get_name());
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(name_label, lv_color_hex(0xffffff), 0);

    const char *title = onboarding_get_title();
    if (title && title[0]) {
        lv_obj_t *title_label = lv_label_create(info);
        lv_label_set_text(title_label, title);
        lv_obj_set_style_text_color(title_label, lv_color_hex(0x63707d), 0);
        lv_obj_set_width(title_label, 138);
        lv_label_set_long_mode(title_label, LV_LABEL_LONG_MODE_WRAP);
    }

    lv_timer_t *timer = lv_timer_create(update_greeting, 1000, NULL);
    update_greeting(timer);
    return timer;
}
