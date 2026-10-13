#include <math.h>
#include <stdio.h>
#include <time.h>

#include "lvgl.h"

#include "watchface_common.h"
#include "watchface_dualtime.h"

#define FACE_BOX_SIZE 132
#define OUTER_RADIUS  60
#define INNER_RADIUS  34
#define FACE_ACCENT   0xd4af6a

#define DEG2RAD_F 0.0174533f

static lv_obj_t *s_time_label;
static lv_obj_t *s_date_label;
static lv_obj_t *s_battery_label;

static void add_numeral_ring(lv_obj_t *box)
{
    for (int i = 0; i < 12; i++) {
        float angle = -90.0f + i * 30.0f;
        float c = cosf(angle * DEG2RAD_F);
        float s = sinf(angle * DEG2RAD_F);
        char buf[4];

        int outer_val = (i == 0) ? 12 : i;
        lv_obj_t *outer_lbl = lv_label_create(box);
        snprintf(buf, sizeof(buf), "%d", outer_val);
        lv_label_set_text(outer_lbl, buf);
        lv_obj_set_style_text_color(outer_lbl, lv_color_hex(0xffffff), 0);
        lv_obj_align(outer_lbl, LV_ALIGN_CENTER, (int32_t)(OUTER_RADIUS * c), (int32_t)(OUTER_RADIUS * s));

        // Only every other hour gets a 24h-ring label (radially under its
        // matching even outer hour) - all 12 crowds badly at this radius
        // given the fonts compiled in (14/20/48 only, see sdkconfig), and
        // even hours are enough to read the ring as "24-hour" at a glance.
        if (i % 2 != 0) {
            continue;
        }
        int inner_val = (i == 0) ? 24 : 12 + i;
        lv_obj_t *inner_lbl = lv_label_create(box);
        snprintf(buf, sizeof(buf), "%d", inner_val);
        lv_label_set_text(inner_lbl, buf);
        lv_obj_set_style_text_color(inner_lbl, lv_color_hex(0x6b7684), 0);
        lv_obj_align(inner_lbl, LV_ALIGN_CENTER, (int32_t)(INNER_RADIUS * c), (int32_t)(INNER_RADIUS * s));
    }
}

static void update_dualtime(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        char time_buf[8];
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
        lv_label_set_text(s_time_label, time_buf);

        char date_buf[16];
        strftime(date_buf, sizeof(date_buf), "%a %d", tm_info);
        lv_label_set_text(s_date_label, date_buf);
    }

    char batt_buf[16];
    watchface_format_battery(batt_buf, sizeof(batt_buf));
    char batt_text[24];
    snprintf(batt_text, sizeof(batt_text), LV_SYMBOL_BATTERY_FULL " %s", batt_buf);
    lv_label_set_text(s_battery_label, batt_text);
}

lv_timer_t *watchface_dualtime_build(lv_obj_t *parent)
{
    lv_obj_t *frame = watchface_build_frame(parent);
    lv_obj_t *box = watchface_build_face_box(frame, FACE_BOX_SIZE);

    add_numeral_ring(box);

    s_time_label = lv_label_create(box);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(FACE_ACCENT), 0);
    lv_obj_center(s_time_label);

    lv_obj_t *info = watchface_build_info_column(frame, 140);

    s_date_label = lv_label_create(info);
    lv_obj_set_style_text_font(s_date_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0xffffff), 0);

    s_battery_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_battery_label, lv_color_hex(0x9fb3c8), 0);

    lv_timer_t *timer = lv_timer_create(update_dualtime, 1000, NULL);
    update_dualtime(timer);
    return timer;
}
