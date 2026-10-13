#include <math.h>
#include <string.h>

#include "eyes.h"

// Base geometry, tuned for the 320x150 content area above the slider band.
#define EYE_W      68
#define EYE_H      78
#define EYE_GAP    52   // distance between the two eyes' inner edges
#define EYE_Y      -5   // centre offset from the root's centre; leaves room for the status strip
#define PUPIL_D    30
#define GLINT_D    8
#define PARALLAX   7    // the whole eye drifts a little with the gaze

// How many thin bars make up a slanted upper lid.
#define LID_SLICES 17

// Eyelashes: three per eye, as angles (degrees above horizontal, measured from
// the eye's outer edge) around its upper outer rim.
#define LASH_COUNT 3
#define LASH_LEN   12
#define LASH_W     3
static const float k_lash_rim_deg[LASH_COUNT] = {18.0f, 44.0f, 70.0f};
static const float k_lash_flick_deg = 14.0f; // each lash leans a little further up than the rim's normal

typedef struct {
    lv_obj_t *body;
    lv_obj_t *pupil;
    lv_obj_t *lid_top;
    lv_obj_t *lid_slice[LID_SLICES]; // the slanted form of lid_top
    lv_obj_t *lid_bot;
    lv_obj_t *lash[LASH_COUNT];
    int side; // -1 left, +1 right
} eye_t;

static bool s_lashes;
static bool s_sig_valid[2]; // see eye_sig_t below: has each eye had a frame applied yet?

static eye_t s_eye[2];

// Alarm red the face blends toward (low battery): a soft salmon-red for the
// eye whites and a deep wine for the pupils, so it reads as a flushed,
// worn-out face rather than a warning light.
#define TINT_EYE_COLOR   0xf08a80
#define TINT_PUPIL_COLOR 0x4a0c0c

static const char_theme_t *s_theme_ref;
static float s_last_tint = -1.0f;

static lv_obj_t *plain_obj(lv_obj_t *parent, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

static void build_eye(eye_t *e, lv_obj_t *root, const char_theme_t *theme, int side)
{
    e->side = side;

    e->body = plain_obj(root, theme->eye);
    lv_obj_set_style_clip_corner(e->body, true, 0);
    // A background-coloured border drawn *after* the children hides the
    // anti-aliased fringe of the eye body, which otherwise leaves a faint
    // ring where a lid covers most of the eye.
    lv_obj_set_style_border_width(e->body, 2, 0);
    lv_obj_set_style_border_color(e->body, lv_color_hex(theme->bg), 0);
    lv_obj_set_style_border_post(e->body, true, 0);

    e->pupil = plain_obj(e->body, theme->pupil);
    lv_obj_set_style_radius(e->pupil, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *glint = plain_obj(e->pupil, theme->glint);
    lv_obj_set_size(glint, GLINT_D, GLINT_D);
    lv_obj_set_style_radius(glint, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(glint, LV_ALIGN_TOP_LEFT, 6, 5);

    // Lids use the background colour so they simply "erase" the eye.
    e->lid_top = plain_obj(e->body, theme->bg);
    for (int i = 0; i < LID_SLICES; i++) {
        e->lid_slice[i] = plain_obj(e->body, theme->bg);
        lv_obj_add_flag(e->lid_slice[i], LV_OBJ_FLAG_HIDDEN);
    }
    e->lid_bot = plain_obj(e->body, theme->bg);

    // Lashes are siblings of the body (not children), because the body clips
    // whatever sticks out of it.
    for (int i = 0; i < LASH_COUNT; i++) {
        lv_obj_t *l = plain_obj(root, theme->eye);
        lv_obj_set_size(l, LASH_W, LASH_LEN);
        lv_obj_set_style_radius(l, LASH_W, 0);
        lv_obj_set_style_transform_pivot_x(l, LASH_W / 2, 0);
        lv_obj_set_style_transform_pivot_y(l, LASH_LEN / 2, 0);
        if (!s_lashes) {
            lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
        }
        e->lash[i] = l;
    }
}

void eyes_build(lv_obj_t *root, const char_theme_t *theme, bool lashes)
{
    s_theme_ref = theme;
    s_lashes = lashes;
    s_last_tint = -1.0f; // force the colours to be applied on the first tick
    s_sig_valid[0] = s_sig_valid[1] = false; // ...and the geometry
    build_eye(&s_eye[0], root, theme, -1);
    build_eye(&s_eye[1], root, theme, +1);
}

// Blends the eye and pupil colours toward the alarm red by `tint` (0..1).
// Only touches the objects when the tint has visibly changed.
static void apply_tint(float tint)
{
    if (s_theme_ref == NULL || fabsf(tint - s_last_tint) < 0.01f) {
        return;
    }
    s_last_tint = tint;
    lv_opa_t mix = (lv_opa_t)(tint * 255.0f);
    lv_color_t eye = lv_color_mix(lv_color_hex(TINT_EYE_COLOR), lv_color_hex(s_theme_ref->eye), mix);
    lv_color_t pupil = lv_color_mix(lv_color_hex(TINT_PUPIL_COLOR), lv_color_hex(s_theme_ref->pupil), mix);
    for (int i = 0; i < 2; i++) {
        lv_obj_set_style_bg_color(s_eye[i].body, eye, 0);
        lv_obj_set_style_bg_color(s_eye[i].pupil, pupil, 0);
        for (int k = 0; k < LASH_COUNT; k++) {
            lv_obj_set_style_bg_color(s_eye[i].lash[k], eye, 0);
        }
    }
}

// Places the three lashes on the upper outer rim of the part of the eye that
// is actually open (so they ride down with the lid in a blink). cx/cy is the
// eye's centre in the face layer, w/h its size, and top_h/bot_h how much the
// lids cover.
static void place_lashes(eye_t *e, int cx, int cy, int w, int h, int top_h, int bot_h)
{
    if (!s_lashes) {
        return;
    }
    int open_h = h - top_h - bot_h;
    if (open_h < 2) {
        open_h = 2;
    }
    float vis_cy = (float)(cy - h / 2 + top_h) + open_h / 2.0f;
    float half_w = w / 2.0f;
    float half_h = open_h / 2.0f;
    int out = e->side; // the outer side is away from the face's centre line

    for (int i = 0; i < LASH_COUNT; i++) {
        float rim = k_lash_rim_deg[i] * 3.14159265f / 180.0f;
        float bx = cx + out * half_w * cosf(rim);
        float by = vis_cy - half_h * sinf(rim);

        // Direction the lash points: out along the rim, flicked upward.
        float dir = (k_lash_rim_deg[i] + k_lash_flick_deg) * 3.14159265f / 180.0f;
        float dx = out * cosf(dir);
        float dy = -sinf(dir);

        float mid_x = bx + dx * LASH_LEN * 0.5f;
        float mid_y = by + dy * LASH_LEN * 0.5f;
        lv_obj_align(e->lash[i], LV_ALIGN_CENTER, (int)(mid_x + (mid_x >= 0 ? 0.5f : -0.5f)),
                     (int)(mid_y + (mid_y >= 0 ? 0.5f : -0.5f)));

        // The lash is a vertical bar; rotate it (clockwise, in 0.1 deg) to lie along `dir`.
        float angle = atan2f(dx, -dy) * 180.0f / 3.14159265f;
        lv_obj_set_style_transform_rotation(e->lash[i], (int32_t)(angle * 10.0f), 0);
    }
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// Everything that decides what an eye looks like, as integers. If this is
// identical to the last frame that was applied, no LVGL call is made at all:
// an idle face, which is most of the time, costs only this arithmetic and a
// memcmp, instead of ~20 style/size/position calls per eye per tick (each of
// which invalidates screen area and makes the display redraw it).
typedef struct {
    int w, h;          // eye body size
    int x, y;          // eye body centre offset in the face layer
    int top_h, bot_h;  // lid cover in px
    int tilt10;        // slanted-lid angle, 0.1 deg (0 = level lid)
    int dome_r;        // lower-lid corner radius (the happy arch)
    int pd;            // pupil diameter
    int pox, poy;      // pupil offset inside the eye
    int pupil_hidden;
} eye_sig_t;

static eye_sig_t s_sig[2];


static void apply_eye(eye_t *e, const char_params_t *p, float gx, float gy, float blink, float dy,
                      const char_layout_t *lay)
{
    const float sc = lay->scale;
    int idx = e->side < 0 ? 0 : 1;

    int w = (int)(EYE_W * p->eye_w * sc);
    float h_scale = p->eye_h;
    if (e->side > 0) {
        // right_open < 1 squeezes just the right eye toward a closed line.
        h_scale *= 0.12f + 0.88f * p->right_open;
    }
    int h = (int)(EYE_H * h_scale * sc);
    if (h < 4) {
        h = 4;
    }

    int cx = (int)(e->side * (EYE_W / 2 + EYE_GAP / 2) * sc);
    int ex = cx + (int)(gx * PARALLAX * sc) + (int)lay->ox;
    int ey = (int)((EYE_Y + gy * PARALLAX + dy) * sc) + (int)lay->oy;

    // Lids. asym > 0 lowers the left lid and raises the right one.
    float asym = p->asym * (e->side < 0 ? 1.0f : -1.0f);
    float top = clampf(p->lid_top + asym * 0.18f, 0.0f, 1.0f);
    float bot = clampf(p->lid_bot, 0.0f, 1.0f);

    // A blink closes both lids toward the middle of whatever is left open.
    float open = clampf(1.0f - top - bot, 0.0f, 1.0f);
    top += open * 0.5f * blink;
    bot += open * 0.5f * blink;

    int top_h = (int)(top * h + 0.5f);
    int bot_h = (int)(bot * h + 0.5f);

    bool slanted = fabsf(p->lid_tilt) > 0.4f && top_h > 0;

    int pd = (int)(PUPIL_D * p->pupil * sc);
    bool pupil_hidden = p->pupil < 0.15f;
    if (pupil_hidden) {
        pd = 2;
    }
    float travel_x = (w - pd) * 0.5f * 0.78f;
    float travel_y = (h - pd) * 0.5f * 0.78f;

    eye_sig_t sig = {
        .w = w, .h = h, .x = ex, .y = ey, .top_h = top_h, .bot_h = bot_h,
        .tilt10 = slanted ? (int)(p->lid_tilt * 10.0f) : 0,
        .dome_r = (int)(p->dome * (w / 2)),
        .pd = pd,
        .pox = (int)(gx * travel_x), .poy = (int)(gy * travel_y),
        .pupil_hidden = pupil_hidden,
    };
    if (s_sig_valid[idx] && memcmp(&sig, &s_sig[idx], sizeof sig) == 0) {
        return; // nothing visible changed since the last frame
    }
    s_sig[idx] = sig;
    s_sig_valid[idx] = true;

    lv_obj_set_size(e->body, w, h);
    lv_obj_set_style_radius(e->body, h < w ? h / 2 : w / 2 + 6, 0);
    lv_obj_align(e->body, LV_ALIGN_CENTER, ex, ey);

    // The upper lid. Level lids are one plain bar. For angry brows the lid is
    // a row of thin bars whose heights step along a slope, so its lower edge
    // slants with the inner end low. (A single big rotated rectangle looked
    // simpler but made LVGL's software renderer crawl.) At zero tilt both
    // forms look identical, so the switch between them cannot be seen.
    if (slanted) {
        // Inner end low: the left eye's inner end is on its right, the right
        // eye's on its left, so the lid grows toward that side.
        float slope = tanf(p->lid_tilt * 3.14159265f / 180.0f) * (e->side < 0 ? 1.0f : -1.0f);
        int sw = (w + LID_SLICES - 1) / LID_SLICES;
        for (int i = 0; i < LID_SLICES; i++) {
            float x_mid = i * sw + sw / 2.0f;
            int sh = (int)(top_h + (x_mid - w / 2.0f) * slope + 0.5f);
            if (sh < 0) {
                sh = 0;
            }
            lv_obj_set_size(e->lid_slice[i], sw + 1, sh);
            lv_obj_align(e->lid_slice[i], LV_ALIGN_TOP_LEFT, i * sw, 0);
            if (sh > 0) {
                lv_obj_clear_flag(e->lid_slice[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(e->lid_slice[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
        lv_obj_add_flag(e->lid_top, LV_OBJ_FLAG_HIDDEN);
    } else {
        for (int i = 0; i < LID_SLICES; i++) {
            lv_obj_add_flag(e->lid_slice[i], LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_set_size(e->lid_top, w + 2, top_h);
        lv_obj_align(e->lid_top, LV_ALIGN_TOP_MID, 0, 0);
        if (top_h > 0) {
            lv_obj_clear_flag(e->lid_top, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(e->lid_top, LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_set_size(e->lid_bot, w + 2, bot_h);
    lv_obj_align(e->lid_bot, LV_ALIGN_BOTTOM_MID, 0, 0);
    // The "happy arch": rounding the lower lid's top corners makes the black
    // bulge up into the eye, leaving a ^ shape instead of a flat squint.
    lv_obj_set_style_radius(e->lid_bot, sig.dome_r, 0);
    if (bot_h > 0) {
        lv_obj_clear_flag(e->lid_bot, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(e->lid_bot, LV_OBJ_FLAG_HIDDEN);
    }

    place_lashes(e, ex, ey, w, h, top_h, bot_h);

    // Pupil, kept inside the part of the eye the body leaves visible.
    if (pupil_hidden) {
        lv_obj_add_flag(e->pupil, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(e->pupil, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_size(e->pupil, pd, pd);
    lv_obj_align(e->pupil, LV_ALIGN_CENTER, sig.pox, sig.poy);
}

void eyes_apply(const char_params_t *p, float gaze_x, float gaze_y, float blink, float dy, const char_layout_t *lay)
{
    apply_tint(p->tint);
    apply_eye(&s_eye[0], p, gaze_x, gaze_y, blink, dy, lay);
    apply_eye(&s_eye[1], p, gaze_x, gaze_y, blink, dy, lay);
}
