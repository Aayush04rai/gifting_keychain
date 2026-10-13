#include <math.h>
#include <string.h>

#include "mouth.h"

#define MOUTH_Y   52   // below the eyes, from the root's centre
#define ARC_D     64

#define TINT_MOUTH_COLOR 0xff5a4f // the red the mouth blends toward when tinted

typedef struct {
    int centre, span, y, arc_opa, d, oh_opa, arc_d, x;
} mouth_sig_t;
static mouth_sig_t s_last;
static bool s_have_last;

static lv_obj_t *s_arc;  // smile / frown
static lv_obj_t *s_oh;   // "o"
static uint32_t s_base_color;
static float s_last_tint = -1.0f;

void mouth_build(lv_obj_t *root, const char_theme_t *theme)
{
    s_base_color = theme->mouth;
    s_last_tint = -1.0f;
    s_have_last = false; // a fresh set of objects needs a first full paint
    s_arc = lv_arc_create(root);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_arc, ARC_D, ARC_D);
    lv_arc_set_rotation(s_arc, 0);
    lv_obj_set_style_arc_width(s_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(theme->mouth), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(s_arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(s_arc, LV_OPA_TRANSP, 0);

    s_oh = lv_obj_create(root);
    lv_obj_remove_style_all(s_oh);
    lv_obj_clear_flag(s_oh, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(s_oh, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_oh, 4, 0);
    lv_obj_set_style_border_color(s_oh, lv_color_hex(theme->mouth), 0);
    lv_obj_set_style_opa(s_oh, LV_OPA_TRANSP, 0);
}

void mouth_apply(const char_params_t *p, float dy, const char_layout_t *lay)
{
    const float sc = lay->scale;
    if (fabsf(p->tint - s_last_tint) >= 0.01f) {
        s_last_tint = p->tint;
        lv_color_t c = lv_color_mix(lv_color_hex(TINT_MOUTH_COLOR), lv_color_hex(s_base_color),
                                    (lv_opa_t)(p->tint * 255.0f));
        lv_obj_set_style_arc_color(s_arc, c, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_oh, c, 0);
    }

    // Arc angles are measured clockwise from 3 o'clock, so 90 deg is straight
    // down: a bottom arc is a smile, a top arc (270) is a frown. The span
    // grows with how strong the expression is.
    float amount = fabsf(p->mouth_curve);
    int span = 14 + (int)(amount * 34);
    int centre = p->mouth_curve >= 0 ? 90 : 270;
    int arc_opa = (int)(p->mouth_alpha * 255);
    int d = (int)((8 + p->mouth_open * 16) * sc);
    int arc_d = (int)(ARC_D * sc);
    int my = (int)((MOUTH_Y + dy) * sc) + (int)lay->oy;
    int mx = (int)lay->ox;
    int oh_opa = (int)(p->mouth_open * 255);

    // As with the eyes: skip every LVGL call unless something visible changed
    // (a hidden mouth, which is most expressions, costs nothing).
    mouth_sig_t sig = {centre, span, my, arc_opa, d, oh_opa, arc_d, mx};
    if (s_have_last && memcmp(&sig, &s_last, sizeof sig) == 0) {
        return;
    }
    s_last = sig;
    s_have_last = true;

    lv_arc_set_bg_angles(s_arc, (uint16_t)(centre - span), (uint16_t)(centre + span));
    lv_obj_set_size(s_arc, arc_d, arc_d);
    lv_obj_set_style_arc_width(s_arc, sc < 0.8f ? 3 : 4, LV_PART_MAIN);
    lv_obj_align(s_arc, LV_ALIGN_CENTER, mx, my - arc_d / 2 + (int)(10 * sc));
    lv_obj_set_style_opa(s_arc, (lv_opa_t)arc_opa, 0);

    lv_obj_set_size(s_oh, d, d);
    lv_obj_align(s_oh, LV_ALIGN_CENTER, mx, my);
    lv_obj_set_style_opa(s_oh, (lv_opa_t)oh_opa, 0);
}
