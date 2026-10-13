#include "lvgl.h"

#include "board_hal.h"
#include "character.h"
#include "ui.h"
#include "watchface_companion.h"

typedef lv_timer_t *(*face_build_fn_t)(lv_obj_t *parent);

#define UI_MAX_FACES 16

static face_build_fn_t s_faces[UI_MAX_FACES];
static uint32_t s_face_count;

// The companion is the only face for now. The earlier watch faces live in
// archive/watchfaces/, outside the build, until they are wanted again.
static void build_face_list(void)
{
    s_face_count = 0;
    s_faces[s_face_count++] = watchface_companion_build;
}

// The content area is shorter than the full 320x172 screen, leaving this
// band free along the bottom edge for the face-position slider so it never
// overlaps a face's own content (every face module centers/anchors its
// layout against whatever height it's given, so shrinking the content
// object is all that's needed here - see watchface_common.c and friends).
// The board has no touch, and the card design has no slider, so the slider is
// hidden and the content uses the whole screen (no band reserved).
#define UI_SLIDER_BAND_H 0
#define UI_SLIDER_ACCENT 0xd4af6a
#define UI_SLIDER_TRACK  0x2a333d

static lv_obj_t *s_content;
static lv_obj_t *s_slider;
static lv_timer_t *s_face_timer;
static uint32_t s_face_index;

// A face's own state (needle/label pointers) is file-local static data in
// its module, so switching faces means fully tearing down the previous
// one's objects and timer, then building the next face fresh - cheap at
// this screen size and it keeps each face module self-contained.
static void show_face(uint32_t index)
{
    if (s_face_timer) {
        lv_timer_delete(s_face_timer);
        s_face_timer = NULL;
        character_leave();
    }
    lv_obj_clean(s_content);
    s_face_timer = s_faces[index](s_content);
    lv_slider_set_value(s_slider, (int32_t)index, LV_ANIM_OFF);
}

// Only fires for a user drag/click on the slider itself -
// lv_slider_set_value() in show_face() above does not send LV_EVENT_VALUE_CHANGED,
// so updating the slider to reflect a gesture-driven face change can't loop
// back into here.
static void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    uint32_t index = (uint32_t)lv_slider_get_value(slider);
    if (index != s_face_index) {
        s_face_index = index;
        show_face(s_face_index);
    }
}

void ui_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    build_face_list();

    s_content = lv_obj_create(scr);
    lv_obj_remove_style_all(s_content);
    lv_obj_set_size(s_content, BOARD_LCD_H_RES, BOARD_LCD_V_RES - UI_SLIDER_BAND_H);
    lv_obj_align(s_content, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

    // Doubles as a face-position indicator (always kept in sync, including
    // when a tilt/button gesture changes the face) and, wherever a pointer
    // input exists - the desktop simulator's mouse; the real board has no
    // touch panel - a drag-to-switch control.
    s_slider = lv_slider_create(scr);
    lv_obj_set_size(s_slider, 260, 8);
    lv_obj_align(s_slider, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_slider_set_range(s_slider, 0, (int32_t)s_face_count - 1);
    lv_obj_set_style_radius(s_slider, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_slider, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(UI_SLIDER_TRACK), LV_PART_MAIN);
    lv_obj_set_style_radius(s_slider, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(UI_SLIDER_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(UI_SLIDER_ACCENT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_slider, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(s_slider, LV_OBJ_FLAG_HIDDEN);

    s_face_index = 0;
    show_face(s_face_index);
}

bool ui_on_companion_face(void)
{
    return s_face_count > 0 && s_faces[s_face_index] == watchface_companion_build;
}

void ui_next_face(void)
{
    if (s_face_count <= 1) {
        return;
    }
    s_face_index = (s_face_index + 1) % s_face_count;
    show_face(s_face_index);
}

void ui_prev_face(void)
{
    if (s_face_count <= 1) {
        return;
    }
    s_face_index = (s_face_index + s_face_count - 1) % s_face_count;
    show_face(s_face_index);
}
