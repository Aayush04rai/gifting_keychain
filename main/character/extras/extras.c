#include <math.h>
#include <string.h>

#include "extras.h"

// What the marks look like this frame, as integers. A mark that is invisible
// contributes zeros to this regardless of its animation phase, so while none of
// them is showing (most of the time) the signature never changes and nothing
// is touched.
typedef struct {
    int q_opa;
    int z_x, z_y, z_opa;
    int sweat_y, sweat_opa;
    int dots_idx, dots_opa;
    int lay_s, lay_x, lay_y;
} extras_sig_t;
static extras_sig_t s_last;
static bool s_have_last;

static lv_obj_t *s_q;
static lv_obj_t *s_z;
static lv_obj_t *s_dots;
static lv_obj_t *s_focus;
static lv_obj_t *s_sweat;

#define SWEAT_COLOR 0x8fd3ff // a cool pale blue, the one cold colour on a red face
#define SWEAT_X     104      // from the centre, beside the right eye
#define SWEAT_Y0    -38
#define SWEAT_DROP  34       // how far the drop slides before it fades

static lv_obj_t *make_label(lv_obj_t *root, const char *text, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *l = lv_label_create(root);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_opa(l, LV_OPA_TRANSP, 0);
    return l;
}

void extras_build(lv_obj_t *root, const char_theme_t *theme)
{
    s_have_last = false; // a fresh set of objects needs a first full paint
    s_q = make_label(root, "?", &lv_font_montserrat_20, theme->mouth);
    lv_obj_align(s_q, LV_ALIGN_CENTER, 112, -42);

    s_z = make_label(root, "z", &lv_font_montserrat_20, theme->extra);
    lv_obj_align(s_z, LV_ALIGN_CENTER, 112, -28);

    s_dots = make_label(root, "", &lv_font_montserrat_20, theme->extra);
    lv_obj_align(s_dots, LV_ALIGN_CENTER, 0, 56);

    // Sweat drop: a small teardrop - a rounded blob with a point is overkill
    // at this size, so it is a narrow rounded rectangle that reads as a drop.
    s_sweat = lv_obj_create(root);
    lv_obj_remove_style_all(s_sweat);
    lv_obj_clear_flag(s_sweat, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s_sweat, 9, 14);
    lv_obj_set_style_radius(s_sweat, 5, 0);
    lv_obj_set_style_bg_color(s_sweat, lv_color_hex(SWEAT_COLOR), 0);
    lv_obj_set_style_bg_opa(s_sweat, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(s_sweat, LV_OPA_TRANSP, 0);
    lv_obj_align(s_sweat, LV_ALIGN_CENTER, SWEAT_X, SWEAT_Y0);

    s_focus = make_label(root, "focused.", &lv_font_montserrat_14, theme->extra);
    lv_obj_set_style_text_letter_space(s_focus, 1, 0);
    lv_obj_align(s_focus, LV_ALIGN_CENTER, 0, 62);
}

void extras_set_focus(float alpha)
{
    lv_obj_set_style_opa(s_focus, (lv_opa_t)(alpha * 255.0f), 0);
}




void extras_apply(const char_params_t *p, uint32_t now_ms, const char_layout_t *lay)
{
    const float sc = lay->scale;
    const int ox = (int)lay->ox, oy = (int)lay->oy;
    extras_sig_t sig = {0};
    sig.lay_s = (int)(sc * 100);
    sig.lay_x = ox;
    sig.lay_y = oy;
    sig.q_opa = (int)(p->extra_q * 255);

    // The z drifts up and back, about 3 s a cycle - slow enough to read as
    // sleep rather than a spinner.
    float phase = (float)(now_ms % 3200) / 3200.0f;
    sig.z_opa = (int)(p->extra_z * 255 * (1.0f - phase * 0.7f));
    if (sig.z_opa > 0) {
        sig.z_x = (int)((112 + phase * 6) * sc) + ox;
        sig.z_y = (int)((-28 - phase * 14) * sc) + oy;
    }

    // The drop forms, slides down the cheek over ~2.4 s and fades as it goes,
    // then another begins.
    float drip = (float)(now_ms % 2400) / 2400.0f;
    sig.sweat_opa = (int)(p->extra_sweat * 255.0f * (1.0f - drip * drip));
    if (sig.sweat_opa > 0) {
        sig.sweat_y = (int)((SWEAT_Y0 + drip * SWEAT_DROP) * sc) + oy;
    }

    sig.dots_opa = (int)(p->extra_dots * 255);
    if (sig.dots_opa > 0) {
        sig.dots_idx = (int)((now_ms / 380) % 3);
    }

    if (s_have_last && memcmp(&sig, &s_last, sizeof sig) == 0) {
        return;
    }
    s_last = sig;
    s_have_last = true;

    lv_obj_align(s_q, LV_ALIGN_CENTER, (int)(112 * sc) + ox, (int)(-42 * sc) + oy);
    lv_obj_set_style_opa(s_q, (lv_opa_t)sig.q_opa, 0);

    if (sig.z_opa > 0) {
        lv_obj_align(s_z, LV_ALIGN_CENTER, sig.z_x, sig.z_y);
    }
    lv_obj_set_style_opa(s_z, (lv_opa_t)sig.z_opa, 0);

    if (sig.sweat_opa > 0) {
        lv_obj_align(s_sweat, LV_ALIGN_CENTER, (int)(SWEAT_X * sc) + ox, sig.sweat_y);
    }
    lv_obj_set_style_opa(s_sweat, (lv_opa_t)sig.sweat_opa, 0);

    static const char *k_dots[] = {".", "..", "..."};
    if (sig.dots_opa > 0) {
        lv_label_set_text(s_dots, k_dots[sig.dots_idx]);
        lv_obj_align(s_dots, LV_ALIGN_CENTER, ox, (int)(56 * sc) + oy);
    }
    lv_obj_set_style_opa(s_dots, (lv_opa_t)sig.dots_opa, 0);
}
