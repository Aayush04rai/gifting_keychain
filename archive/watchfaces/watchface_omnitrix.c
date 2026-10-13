#include <stdio.h>
#include <time.h>

#include "lvgl.h"

#include "watchface_common.h"
#include "watchface_omnitrix.h"

#define FACE_BOX_SIZE 132
#define RING_DIA      100
#define RING_W        8
#define DISC_DIA      78
#define BAR_W         30
#define LIGHT_DIA     10

#define RING_CLR   0x555a60
#define RING_BG    0x1a1a1a
#define DISC_CLR   0x9be23c
#define LIGHT_CLR  0xb6ff3c

static lv_obj_t *s_time_label;
static lv_obj_t *s_date_label;

static void add_light(lv_obj_t *ring, lv_align_t align, int32_t x, int32_t y)
{
    lv_obj_t *light = lv_obj_create(ring);
    lv_obj_remove_style_all(light);
    lv_obj_set_size(light, LIGHT_DIA, LIGHT_DIA);
    lv_obj_align(light, align, x, y);
    lv_obj_set_style_radius(light, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(light, lv_color_hex(LIGHT_CLR), 0);
    lv_obj_set_style_bg_opa(light, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(light, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(light, 2, 0);
    lv_obj_clear_flag(light, LV_OBJ_FLAG_SCROLLABLE);
}

static void update_omnitrix(lv_timer_t *timer)
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
}

lv_timer_t *watchface_omnitrix_build(lv_obj_t *parent)
{
    lv_obj_t *frame = watchface_build_frame(parent);
    lv_obj_t *box = watchface_build_face_box(frame, FACE_BOX_SIZE);

    lv_obj_t *ring = lv_obj_create(box);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, RING_DIA, RING_DIA);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ring, lv_color_hex(RING_BG), 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(RING_CLR), 0);
    lv_obj_set_style_border_width(ring, RING_W, 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);

    add_light(ring, LV_ALIGN_TOP_MID, 0, -6);
    add_light(ring, LV_ALIGN_BOTTOM_MID, 0, 6);
    add_light(ring, LV_ALIGN_LEFT_MID, -6, 0);
    add_light(ring, LV_ALIGN_RIGHT_MID, 6, 0);

    // Green disc with a black hourglass/"X" cut across it: two thick
    // diagonal bars clipped to the disc's circular boundary via
    // clip_corner, leaving green showing through top/bottom/left/right -
    // the Omnitrix's signature silhouette, built from plain shapes only.
    lv_obj_t *disc = lv_obj_create(ring);
    lv_obj_remove_style_all(disc);
    lv_obj_set_size(disc, DISC_DIA, DISC_DIA);
    lv_obj_center(disc);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(disc, true, 0);
    lv_obj_set_style_bg_color(disc, lv_color_hex(DISC_CLR), 0);
    lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_SCROLLABLE);

    // lv_line draws points relative to the object's own top-left corner,
    // not centered on (0,0) - so the points must start at (0,0) and the
    // object (auto-sized to their bounding box) is what gets centered.
    static lv_point_precise_t bar1_pts[] = {{0, 0}, {120, 120}};
    static lv_point_precise_t bar2_pts[] = {{120, 0}, {0, 120}};

    lv_obj_t *bar1 = lv_line_create(disc);
    lv_obj_remove_style_all(bar1);
    lv_line_set_points(bar1, bar1_pts, 2);
    lv_obj_set_style_line_color(bar1, lv_color_hex(0x111111), 0);
    lv_obj_set_style_line_width(bar1, BAR_W, 0);
    lv_obj_set_style_line_opa(bar1, LV_OPA_COVER, 0);
    lv_obj_center(bar1);

    lv_obj_t *bar2 = lv_line_create(disc);
    lv_obj_remove_style_all(bar2);
    lv_line_set_points(bar2, bar2_pts, 2);
    lv_obj_set_style_line_color(bar2, lv_color_hex(0x111111), 0);
    lv_obj_set_style_line_width(bar2, BAR_W, 0);
    lv_obj_set_style_line_opa(bar2, LV_OPA_COVER, 0);
    lv_obj_center(bar2);

    lv_obj_t *info = watchface_build_info_column(frame, 140);

    s_time_label = lv_label_create(info);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(0xffffff), 0);

    s_date_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0x6b7684), 0);

    lv_timer_t *timer = lv_timer_create(update_omnitrix, 1000, NULL);
    update_omnitrix(timer);
    return timer;
}
