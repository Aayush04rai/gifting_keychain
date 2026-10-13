#include <math.h>
#include <stdio.h>
#include <time.h>

#include "lvgl.h"

#include "watchface_common.h"
#include "watchface_orbit.h"

#define FACE_BOX_SIZE 120
#define FACE_ACCENT   0xd4af6a

#define DEG2RAD_F 0.0174533f

static lv_obj_t *s_time_label;
static lv_obj_t *s_date_label;
static lv_obj_t *s_battery_label;

static void add_ring(lv_obj_t *box, int32_t size, uint32_t color, int32_t width)
{
    lv_obj_t *ring = lv_obj_create(box);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, size, size);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(ring, width, 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
}

// A small filled dot "orbiting" the rings at a fixed angle/radius - purely
// decorative, echoing the reference art's chain of planet-like circles.
static void add_planet(lv_obj_t *box, float angle_deg, int32_t radius, int32_t dia, uint32_t color)
{
    lv_obj_t *dot = lv_obj_create(box);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, dia, dia);
    int32_t dx = (int32_t)(radius * cosf(angle_deg * DEG2RAD_F));
    int32_t dy = (int32_t)(radius * sinf(angle_deg * DEG2RAD_F));
    lv_obj_align(dot, LV_ALIGN_CENTER, dx, dy);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
}

static void update_orbit(lv_timer_t *timer)
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

lv_timer_t *watchface_orbit_build(lv_obj_t *parent)
{
    lv_obj_t *frame = watchface_build_frame(parent);
    lv_obj_t *box = watchface_build_face_box(frame, FACE_BOX_SIZE);

    add_ring(box, 104, 0x4a3018, 3);
    add_ring(box, 80, 0x6b4a20, 2);
    add_ring(box, 56, FACE_ACCENT, 2);
    add_ring(box, 32, 0x8a97a6, 1);

    add_planet(box, -75.0f, 52, 10, 0x7fc8e8);
    add_planet(box, -35.0f, 56, 8, 0xe89a9a);
    add_planet(box, 5.0f, 54, 9, 0xb98bd6);
    add_planet(box, 40.0f, 50, 7, 0x9fc46b);
    add_planet(box, 70.0f, 46, 6, 0xc0392b);

    lv_obj_t *info = watchface_build_info_column(frame, 140);

    s_time_label = lv_label_create(info);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(FACE_ACCENT), 0);

    s_date_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0xffffff), 0);

    s_battery_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_battery_label, lv_color_hex(0x9fb3c8), 0);

    lv_timer_t *timer = lv_timer_create(update_orbit, 1000, NULL);
    update_orbit(timer);
    return timer;
}
