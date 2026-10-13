#include <stdio.h>

#include "lvgl.h"

#include "cricket.h"
#include "watchface_common.h"
#include "watchface_cricket.h"

#define FACE_BOX_SIZE 120
#define FACE_ACCENT   0xd4af6a

static lv_obj_t *s_team1_label;
static lv_obj_t *s_team2_label;
static lv_obj_t *s_score1_label;
static lv_obj_t *s_score2_label;
static lv_obj_t *s_status_label;

// First 3 letters, upper-cased, as a compact team code - "India" -> "IND".
static void short_code(const char *name, char *out, size_t out_size)
{
    size_t i = 0;
    for (; i < out_size - 1 && name[i]; i++) {
        char c = name[i];
        out[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    }
    out[i] = '\0';
}

static void update_cricket(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    cricket_score_t score;
    if (!cricket_get(&score)) {
        lv_label_set_text(s_team1_label, "--");
        lv_label_set_text(s_team2_label, "");
        lv_label_set_text(s_score1_label, "Fetching live score...");
        lv_label_set_text(s_score2_label, "");
        lv_label_set_text(s_status_label, "");
        return;
    }

    char code1[4];
    char code2[4];
    short_code(score.team1, code1, sizeof(code1));
    short_code(score.team2, code2, sizeof(code2));
    lv_label_set_text(s_team1_label, code1);
    lv_label_set_text(s_team2_label, code2);

    char line1[40];
    char line2[40];
    snprintf(line1, sizeof(line1), "%s %s %s", code1, score.score1, score.overs1);
    snprintf(line2, sizeof(line2), "%s %s %s", code2, score.score2, score.overs2);
    lv_label_set_text(s_score1_label, line1);
    lv_label_set_text(s_score2_label, line2);
    lv_label_set_text(s_status_label, score.status);
}

lv_timer_t *watchface_cricket_build(lv_obj_t *parent)
{
    lv_obj_t *frame = watchface_build_frame(parent);
    lv_obj_t *box = watchface_build_face_box(frame, FACE_BOX_SIZE);

    lv_obj_t *col = lv_obj_create(box);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);

    s_team1_label = lv_label_create(col);
    lv_obj_set_style_text_font(s_team1_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_team1_label, lv_color_hex(0xffffff), 0);

    lv_obj_t *vs_label = lv_label_create(col);
    lv_label_set_text(vs_label, "vs");
    lv_obj_set_style_text_color(vs_label, lv_color_hex(FACE_ACCENT), 0);

    s_team2_label = lv_label_create(col);
    lv_obj_set_style_text_font(s_team2_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_team2_label, lv_color_hex(0xffffff), 0);

    lv_obj_t *info = watchface_build_info_column(frame, 140);

    s_score1_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_score1_label, lv_color_hex(0xffffff), 0);

    s_score2_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_score2_label, lv_color_hex(0xffffff), 0);

    s_status_label = lv_label_create(info);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x9fb3c8), 0);
    lv_obj_set_width(s_status_label, 138);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_MODE_WRAP);

    lv_timer_t *timer = lv_timer_create(update_cricket, 2000, NULL);
    update_cricket(timer);
    return timer;
}
