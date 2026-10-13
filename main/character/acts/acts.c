#include <math.h>
#include <string.h>

#include "acts.h"

#define PI_F 3.14159265f

// All positions are offsets from the centre of the face layer (320 x 150).
// The eyes sit around y = -5 and span about +-39 px, with a 52 px gap between
// them; the space under the eyes and the gap between them are where props go.

static act_id_t s_act;
static act_id_t s_last_act;
static uint32_t s_t0;
static bool s_hit;      // paper act: does the throw go in?
static int s_shape;     // doodle act: 0 heart, 1 star, 2 spiral

static uint32_t s_gold;
static uint32_t s_light;
static uint32_t s_navy;

// --- helpers --------------------------------------------------------------

static lv_obj_t *plain(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

static lv_obj_t *blob(lv_obj_t *parent, int w, int h, int radius, uint32_t color)
{
    lv_obj_t *o = plain(parent);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    return o;
}

static void show(lv_obj_t *o, bool on)
{
    if (on) {
        lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
}

static void place(lv_obj_t *o, float x, float y)
{
    lv_obj_align(o, LV_ALIGN_CENTER, (int)(x + (x >= 0 ? 0.5f : -0.5f)), (int)(y + (y >= 0 ? 0.5f : -0.5f)));
}

static float clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// Fades a prop in over `fade_ms` starting at `from_ms` (all relative to the
// act's start).
static void fade_in(lv_obj_t *o, uint32_t t, uint32_t from_ms, uint32_t fade_ms)
{
    if (t < from_ms) {
        show(o, false);
        return;
    }
    show(o, true);
    lv_obj_set_style_opa(o, (lv_opa_t)(255.0f * clamp01((float)(t - from_ms) / (float)fade_ms)), 0);
}

// --- props ----------------------------------------------------------------

// Huff: two puffs of steam, one each side of the face.
static lv_obj_t *s_puff[2];

// Doodle: one polyline, revealed point by point.
#define DOODLE_MAX_PTS 64
static lv_obj_t *s_line;
static lv_point_precise_t s_pts[DOODLE_MAX_PTS];
static int s_pts_n;

// Rocket: a small group - body, nose, two fins, a window and a flame.
static lv_obj_t *s_rocket;
static lv_obj_t *s_r_body, *s_r_nose, *s_r_fin_l, *s_r_fin_r, *s_r_window, *s_r_flame;

// Paper: a bin to aim at and the ball.
static lv_obj_t *s_bin;
static lv_obj_t *s_ball;

// Exercise: bar, four plates, two arms.
static lv_obj_t *s_dumb_bar, *s_dumb_plate[4], *s_dumb_arm[2];

void acts_build(lv_obj_t *face_layer, const char_theme_t *theme)
{
    s_gold = theme->mouth;
    s_light = theme->eye;
    s_navy = theme->pupil;
    s_act = ACT_NONE;

    for (int i = 0; i < 2; i++) {
        s_puff[i] = blob(face_layer, 10, 10, LV_RADIUS_CIRCLE, 0xb8c4cf);
    }

    s_line = lv_line_create(face_layer);
    lv_obj_set_style_line_width(s_line, 3, 0);
    lv_obj_set_style_line_color(s_line, lv_color_hex(s_gold), 0);
    lv_obj_set_style_line_rounded(s_line, true, 0);
    lv_obj_add_flag(s_line, LV_OBJ_FLAG_HIDDEN);

    // The rocket is built in its own 24 x 52 box so it can be moved as one.
    s_rocket = plain(face_layer);
    lv_obj_set_size(s_rocket, 24, 52);
    lv_obj_set_style_bg_opa(s_rocket, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(s_rocket, LV_OBJ_FLAG_HIDDEN);
    s_r_flame = blob(s_rocket, 8, 15, 4, 0xf5d99a);
    lv_obj_align(s_r_flame, LV_ALIGN_TOP_MID, 0, 36);
    s_r_fin_l = blob(s_rocket, 7, 13, 2, s_gold);
    lv_obj_align(s_r_fin_l, LV_ALIGN_TOP_LEFT, 0, 24);
    s_r_fin_r = blob(s_rocket, 7, 13, 2, s_gold);
    lv_obj_align(s_r_fin_r, LV_ALIGN_TOP_RIGHT, 0, 24);
    s_r_body = blob(s_rocket, 14, 34, 7, s_light);
    lv_obj_align(s_r_body, LV_ALIGN_TOP_MID, 0, 5);
    s_r_nose = blob(s_rocket, 10, 10, LV_RADIUS_CIRCLE, s_gold);
    lv_obj_align(s_r_nose, LV_ALIGN_TOP_MID, 0, 0);
    s_r_window = blob(s_rocket, 6, 6, LV_RADIUS_CIRCLE, s_navy);
    lv_obj_align(s_r_window, LV_ALIGN_TOP_MID, 0, 16);

    s_bin = plain(face_layer);
    lv_obj_set_size(s_bin, 28, 32);
    lv_obj_set_style_bg_opa(s_bin, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(s_bin, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *cup = blob(s_bin, 22, 28, 3, 0x3a444e);
    lv_obj_align(cup, LV_ALIGN_BOTTOM_MID, 0, 0);
    show(cup, true);
    lv_obj_t *rim = blob(s_bin, 28, 5, 2, 0x6b7885);
    lv_obj_align(rim, LV_ALIGN_TOP_MID, 0, 0);
    show(rim, true);

    s_ball = blob(face_layer, 11, 11, LV_RADIUS_CIRCLE, s_light);

    s_dumb_bar = blob(face_layer, 10, 4, 2, 0xc8d2e0);
    for (int i = 0; i < 4; i++) {
        s_dumb_plate[i] = blob(face_layer, 6, 10, 2, i < 2 ? s_gold : 0x8a7440);
    }
    for (int i = 0; i < 2; i++) {
        s_dumb_arm[i] = blob(face_layer, 10, 10, LV_RADIUS_CIRCLE, 0xdfe6ee); // a hand gripping the bar
    }
}

static int s_dumb_sig[5];
static bool s_dumb_on;

void acts_exercise(bool on, float lift, const char_layout_t *lay)
{
    if (!on || !lay) {
        if (s_dumb_on) {
            s_dumb_on = false;
            show(s_dumb_bar, false);
            for (int i = 0; i < 4; i++) {
                show(s_dumb_plate[i], false);
            }
            show(s_dumb_arm[0], false);
            show(s_dumb_arm[1], false);
        }
        return;
    }
    const float sc = lay->scale;
    int half = (int)(78 * sc);                       // bar half-length
    int bar_y = (int)lay->oy - (int)(62 * sc) - (int)(lift * 16.0f);
    int face_mid = (int)lay->oy + (int)(8 * sc);     // where the hands' arms end
    int sig[5] = {half, bar_y, (int)lay->ox, face_mid, 0};
    if (s_dumb_on && memcmp(sig, s_dumb_sig, sizeof sig) == 0) {
        return;
    }
    memcpy(s_dumb_sig, sig, sizeof sig);
    if (!s_dumb_on) {
        s_dumb_on = true;
        show(s_dumb_bar, true);
        for (int i = 0; i < 4; i++) {
            show(s_dumb_plate[i], true);
        }
        show(s_dumb_arm[0], true);
        show(s_dumb_arm[1], true);
    }
    int ox = (int)lay->ox;
    lv_obj_set_size(s_dumb_bar, half * 2, 4);
    place(s_dumb_bar, ox, bar_y);
    for (int side = 0; side < 2; side++) {
        int sgn = side == 0 ? -1 : 1;
        int plate_h = (int)(34 * sc), inner_h = (int)(26 * sc);
        lv_obj_set_size(s_dumb_plate[side], 6, plate_h);
        place(s_dumb_plate[side], ox + sgn * half, bar_y);
        lv_obj_set_size(s_dumb_plate[2 + side], 5, inner_h);
        place(s_dumb_plate[2 + side], ox + sgn * (half - 7), bar_y);
        place(s_dumb_arm[side], ox + sgn * (int)(46 * sc), bar_y);
    }
}

static void hide_all(void)
{
    show(s_puff[0], false);
    show(s_puff[1], false);
    show(s_line, false);
    show(s_rocket, false);
    show(s_bin, false);
    show(s_ball, false);
    acts_exercise(false, 0.0f, NULL);
    lv_obj_set_style_opa(s_line, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(s_bin, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(s_ball, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(s_rocket, LV_OPA_COVER, 0);
}

// --- doodle shapes ----------------------------------------------------------

static void build_shape(int shape)
{
    float xs[DOODLE_MAX_PTS], ys[DOODLE_MAX_PTS];
    int n = 0;

    if (shape == 0) { // heart
        n = 48;
        for (int k = 0; k < n; k++) {
            float t = 2.0f * PI_F * (float)k / (float)(n - 1);
            xs[k] = 1.25f * 16.0f * powf(sinf(t), 3.0f);
            ys[k] = -1.25f * (13.0f * cosf(t) - 5.0f * cosf(2 * t) - 2.0f * cosf(3 * t) - cosf(4 * t));
        }
    } else if (shape == 1) { // five-pointed star in a single stroke
        const int per_seg = 8;
        float vx[6], vy[6];
        for (int k = 0; k < 6; k++) {
            float a = (-90.0f + 144.0f * (float)k) * PI_F / 180.0f;
            vx[k] = 22.0f * cosf(a);
            vy[k] = 22.0f * sinf(a);
        }
        for (int seg = 0; seg < 5; seg++) {
            for (int j = 0; j < per_seg; j++) {
                float u = (float)j / (float)per_seg;
                xs[n] = vx[seg] + (vx[seg + 1] - vx[seg]) * u;
                ys[n] = vy[seg] + (vy[seg + 1] - vy[seg]) * u;
                n++;
            }
        }
        xs[n] = vx[5];
        ys[n] = vy[5];
        n++;
    } else { // spiral
        n = 60;
        for (int k = 0; k < n; k++) {
            float th = 6.0f * PI_F * (float)k / (float)(n - 1);
            float r = 0.95f * th;
            xs[k] = r * cosf(th);
            ys[k] = r * sinf(th);
        }
    }

    // Shift so the shape starts at the object's top-left corner (the line
    // widget places points relative to itself).
    float minx = xs[0], miny = ys[0];
    for (int k = 1; k < n; k++) {
        if (xs[k] < minx) minx = xs[k];
        if (ys[k] < miny) miny = ys[k];
    }
    for (int k = 0; k < n; k++) {
        s_pts[k].x = (lv_value_precise_t)(xs[k] - minx + 2.0f);
        s_pts[k].y = (lv_value_precise_t)(ys[k] - miny + 2.0f);
    }
    s_pts_n = n;
}

// --- API ------------------------------------------------------------------------

act_id_t acts_pick(companion_t who)
{
    act_id_t a = ACT_NONE;
    do {
        if (who == COMPANION_FEMALE) {
            a = lv_rand(0, 1) ? ACT_HUFF : ACT_DOODLE;
        } else {
            a = lv_rand(0, 1) ? ACT_ROCKET : ACT_PAPER;
        }
        // Don't repeat the previous act - but if there is only ever the one
        // choice left to avoid it would loop, so a coin flip settles it.
    } while (a == s_last_act && lv_rand(0, 2) != 0);
    return a;
}

bool acts_start(act_id_t id, uint32_t now_ms)
{
    if (s_act != ACT_NONE || id == ACT_NONE) {
        return false;
    }
    hide_all();
    s_act = id;
    s_last_act = id;
    s_t0 = now_ms;
    s_hit = lv_rand(0, 9) < 6; // the throw goes in six times in ten
    s_shape = (int)lv_rand(0, 2);
    if (id == ACT_DOODLE) {
        build_shape(s_shape);
        lv_line_set_points(s_line, s_pts, 2);
        place(s_line, 0, 56);
    }
    return true;
}

void acts_cancel(void)
{
    s_act = ACT_NONE;
    hide_all();
}

static void finish(act_out_t *out, char_emotion_t settle)
{
    s_act = ACT_NONE;
    hide_all();
    out->emotion = settle;
    out->has_gaze = false;
}

// --- the acts ---------------------------------------------------------------------

#define HUFF_MS   3900
#define DOODLE_MS 7000
#define ROCKET_MS 6000
#define PAPER_MS  5200

static bool tick_huff(uint32_t t, act_out_t *out)
{
    if (t >= HUFF_MS) {
        finish(out, CHAR_EMOTION_CALM); // a sigh, and over it
        return false;
    }
    out->emotion = CHAR_EMOTION_ANGRY;
    out->has_gaze = true;
    // Looks pointedly away, steals a glance to see if anyone noticed, looks
    // away again.
    if (t > 1700 && t < 2400) {
        out->gaze_x = 0.0f;
        out->gaze_y = 0.05f;
    } else {
        out->gaze_x = -0.85f;
        out->gaze_y = 0.15f;
    }
    // Steam: a puff off each side, rising and fading, staggered.
    for (int i = 0; i < 2; i++) {
        if (t < 300 || t > HUFF_MS - 500) {
            show(s_puff[i], false);
            continue;
        }
        float phase = (float)((t + (uint32_t)i * 450) % 900) / 900.0f;
        int d = (int)(8.0f + 7.0f * phase);
        lv_obj_set_size(s_puff[i], d, d);
        place(s_puff[i], i == 0 ? -118.0f : 118.0f, -6.0f - 34.0f * phase);
        lv_obj_set_style_opa(s_puff[i], (lv_opa_t)(200.0f * (1.0f - phase)), 0);
        show(s_puff[i], true);
    }
    return true;
}

static bool tick_doodle(uint32_t t, act_out_t *out)
{
    if (t >= DOODLE_MS) {
        finish(out, CHAR_EMOTION_IDLE);
        return false;
    }
    const uint32_t draw_ms = 4600;
    out->has_gaze = true;
    if (t < draw_ms) {
        float p = clamp01((float)t / (float)draw_ms);
        int n = 2 + (int)(p * (float)(s_pts_n - 2));
        lv_line_set_points(s_line, s_pts, (uint32_t)n);
        place(s_line, 0, 56);
        show(s_line, true);
        out->emotion = CHAR_EMOTION_CALM;
        out->gaze_x = 0.0f;
        out->gaze_y = 0.85f; // head down, concentrating
    } else if (t < 6200) {
        out->emotion = CHAR_EMOTION_HAPPY; // admiring it
        out->gaze_x = 0.0f;
        out->gaze_y = 0.5f;
    } else {
        out->emotion = CHAR_EMOTION_HAPPY;
        out->gaze_x = 0.0f;
        out->gaze_y = 0.0f; // looks up, pleased with itself
        float f = clamp01((float)(t - 6200) / 800.0f);
        lv_obj_set_style_opa(s_line, (lv_opa_t)(255.0f * (1.0f - f)), 0);
    }
    return true;
}

static bool tick_rocket(uint32_t t, act_out_t *out)
{
    if (t >= ROCKET_MS) {
        finish(out, CHAR_EMOTION_IDLE);
        return false;
    }
    out->has_gaze = true;
    float rx = 0.0f, ry = 30.0f; // between the eyes

    if (t < 2500) { // assembling, one part at a time
        out->emotion = CHAR_EMOTION_CURIOUS;
        out->gaze_x = 0.0f;
        out->gaze_y = 0.4f;
        show(s_rocket, true);
        fade_in(s_r_body, t, 0, 400);
        fade_in(s_r_fin_l, t, 700, 250);
        fade_in(s_r_fin_r, t, 900, 250);
        fade_in(s_r_nose, t, 1400, 300);
        fade_in(s_r_window, t, 1900, 300);
        show(s_r_flame, false);
    } else if (t < 3200) { // countdown: flame lights, the rocket trembles
        out->emotion = CHAR_EMOTION_SMIRK;
        out->gaze_x = 0.0f;
        out->gaze_y = 0.3f;
        rx = ((t / 60) % 2) ? 1.5f : -1.5f;
        show(s_r_flame, true);
        lv_obj_set_size(s_r_flame, 8, 8 + (int)((t / 50) % 5));
    } else if (t < 4500) { // launch
        float u = (float)(t - 3200) / 1300.0f;
        ry = 30.0f - 190.0f * u * u;
        out->emotion = CHAR_EMOTION_SURPRISED;
        out->gaze_x = 0.0f;
        out->gaze_y = -clamp01(u * 2.0f);
        show(s_r_flame, true);
        lv_obj_set_size(s_r_flame, 8, 13 + (int)((t / 40) % 6));
    } else { // gone: watches it go, then looks back, pleased
        show(s_rocket, false);
        out->emotion = CHAR_EMOTION_HAPPY;
        out->gaze_x = 0.0f;
        out->gaze_y = t < 5200 ? -0.7f : 0.0f;
    }
    place(s_rocket, rx, ry);
    return true;
}

static bool tick_paper(uint32_t t, act_out_t *out)
{
    if (t >= PAPER_MS) {
        finish(out, CHAR_EMOTION_IDLE);
        return false;
    }
    out->has_gaze = true;
    const float bin_x = 108.0f, bin_y = 36.0f;
    const float ball_x0 = 0.0f, ball_y0 = 54.0f;
    // Where the ball ends up: in the bin, or short of it on the floor.
    const float tx = s_hit ? bin_x : bin_x - 30.0f;
    const float ty = s_hit ? bin_y - 6.0f : 62.0f;

    fade_in(s_bin, t, 0, 300);
    place(s_bin, bin_x, bin_y);

    float bx = ball_x0, by = ball_y0;
    bool ball_visible = t >= 300;

    if (t < 1100) { // winds up, eyeing the bin
        out->emotion = CHAR_EMOTION_SMIRK;
        out->gaze_x = 0.9f;
        out->gaze_y = 0.0f;
    } else if (t < 2000) { // the throw
        float u = (float)(t - 1100) / 900.0f;
        bx = ball_x0 + (tx - ball_x0) * u;
        by = ball_y0 + (ty - ball_y0) * u - 4.0f * 46.0f * u * (1.0f - u);
        out->emotion = CHAR_EMOTION_SMIRK;
        out->gaze_x = clamp01(bx / 110.0f);
        out->gaze_y = -0.25f * 4.0f * u * (1.0f - u);
    } else { // the verdict
        bx = tx;
        by = ty;
        if (s_hit) {
            ball_visible = false; // it is in the bin
            out->emotion = t < 3800 ? CHAR_EMOTION_HAPPY : CHAR_EMOTION_SMIRK;
            out->gaze_x = 0.7f;
            out->gaze_y = 0.0f;
            if (t < 2500) { // the bin takes the hit
                place(s_bin, bin_x + ((t / 50) % 2 ? 2.0f : -2.0f), bin_y);
            }
        } else {
            // A little roll, then a look of "I meant that".
            float roll = clamp01((float)(t - 2000) / 700.0f);
            bx = tx - 12.0f * roll;
            out->emotion = t < 2900 ? CHAR_EMOTION_SURPRISED : CHAR_EMOTION_SMIRK;
            out->gaze_x = 0.6f;
            out->gaze_y = 0.25f;
        }
        if (t > 4500) { // props fade away
            float f = clamp01((float)(t - 4500) / 700.0f);
            lv_obj_set_style_opa(s_bin, (lv_opa_t)(255.0f * (1.0f - f)), 0);
            lv_obj_set_style_opa(s_ball, (lv_opa_t)(255.0f * (1.0f - f)), 0);
        }
    }

    show(s_ball, ball_visible);
    place(s_ball, bx, by);
    return true;
}

bool acts_tick(uint32_t now_ms, act_out_t *out)
{
    out->emotion = CHAR_EMOTION_IDLE;
    out->has_gaze = false;
    if (s_act == ACT_NONE) {
        return false;
    }
    uint32_t t = now_ms - s_t0;
    switch (s_act) {
    case ACT_HUFF:
        return tick_huff(t, out);
    case ACT_DOODLE:
        return tick_doodle(t, out);
    case ACT_ROCKET:
        return tick_rocket(t, out);
    case ACT_PAPER:
        return tick_paper(t, out);
    default:
        finish(out, CHAR_EMOTION_IDLE);
        return false;
    }
}
