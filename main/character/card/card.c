#include <string.h>

#include "card.h"

// The card's background: a deep navy that darkens toward the bottom. The news
// artwork on the SD card (tools/make_art.py) is transparent, so it sits on
// this gradient with no visible edge.
#define CARD_BG_TOP 0x0e1626
#define CARD_BG_BOT 0x080c15

// News layout: text column on the left, artwork panel on the right.
#define NEWS_ART_Y     0   // the 176x172 artwork fills the right of the card
#define NEWS_X         16
#define NEWS_CHIP_Y    32
#define NEWS_HEAD_Y    54
#define NEWS_HEAD_W_ART  156
#define NEWS_HEAD_W_FULL 288
#define NEWS_HEAD_H    72  // four lines of the 16 px font; a longer headline scrolls inside this window
#define NEWS_META_Y    142

#define CARD_MARGIN_X 14
#define CARD_HEADER_Y 28
#define CARD_RULE_Y   48
#define CARD_TEXT_Y   58
#define CARD_TEXT_W   292
#define CARD_TEXT_H   72 // three lines of the 20 px font

// The side ("talking") layout: a bubble on the right of the screen.
#define SIDE_X 146
#define SIDE_Y 34
#define SIDE_W 168
#define SIDE_H 108
#define SIDE_TEXT_X 158
#define SIDE_TEXT_W 144

#define OPT_COUNT 3
#define OPT_W     88
#define OPT_H     28
#define OPT_GAP   14
#define OPT_Y     102
// Side layout pills.
#define SOPT_W 46
#define SOPT_H 24
#define SOPT_GAP 3
#define SOPT_Y 112

static lv_obj_t *s_layer;
#define PAGE_DOTS_MAX 6
static lv_obj_t *s_dots[PAGE_DOTS_MAX];
static uint32_t s_news_accent = 0x5b8cff;
static lv_obj_t *s_greet;
static lv_obj_t *s_bubble;
static bool s_side;
static lv_obj_t *s_header;
static lv_obj_t *s_text;
static lv_obj_t *s_hint;
static lv_obj_t *s_rule;
// News layout parts.
static lv_obj_t *s_art;
static lv_obj_t *s_chip;
static lv_obj_t *s_chip_label;
static lv_obj_t *s_head_box; // the window the headline scrolls inside
static lv_obj_t *s_headline;
static int s_scroll_overflow; // px the headline is taller than its window (0 = fits)
static lv_obj_t *s_meta;
static lv_obj_t *s_opt[OPT_COUNT];
static lv_obj_t *s_opt_label[OPT_COUNT];
static uint32_t s_accent;
static uint32_t s_bg;
static uint32_t s_dim;

static void show(lv_obj_t *o, bool on);

static lv_obj_t *plain(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

// A headline longer than its four-line window is shown a window at a time: it
// rests on the first lines, then every few seconds jumps to the next window and
// finally back to the start. Jumping (not gliding) is deliberate: a smooth scroll
// repaints the text over the gradient and the artwork 30 times a second, which
// the display cannot keep up with; a jump repaints once every few seconds.
#define HEAD_PAGE_MS 3200
static lv_timer_t *s_scroll_timer;
static int s_scroll_offset;

static int headline_line_h(void)
{
    return (int)lv_font_get_line_height(&lv_font_montserrat_16);
}

static void headline_scroll_cb(lv_timer_t *t)
{
    (void)t;
    int page = (NEWS_HEAD_H / headline_line_h()) * headline_line_h(); // whole lines per window
    if (s_scroll_offset >= s_scroll_overflow) {
        s_scroll_offset = 0; // after the last window, back to the top
    } else {
        s_scroll_offset += page;
        if (s_scroll_offset > s_scroll_overflow) {
            s_scroll_offset = s_scroll_overflow; // the last window ends flush with the text
        }
    }
    lv_obj_set_y(s_headline, -s_scroll_offset);
}

static void headline_scroll_stop(void)
{
    if (s_scroll_timer) {
        lv_timer_delete(s_scroll_timer);
        s_scroll_timer = NULL;
    }
    s_scroll_offset = 0;
    lv_obj_set_y(s_headline, 0);
}

static void headline_scroll_start(void)
{
    headline_scroll_stop();
    if (s_scroll_overflow <= 0) {
        return;
    }
    s_scroll_timer = lv_timer_create(headline_scroll_cb, HEAD_PAGE_MS, NULL);
}

void card_build(lv_obj_t *root, const char_theme_t *theme)
{
    s_layer = plain(root);
    lv_obj_set_size(s_layer, lv_pct(100), lv_pct(100));
    lv_obj_center(s_layer);
    lv_obj_set_style_bg_color(s_layer, lv_color_hex(CARD_BG_TOP), 0);
    lv_obj_set_style_bg_grad_color(s_layer, lv_color_hex(CARD_BG_BOT), 0);
    lv_obj_set_style_bg_grad_dir(s_layer, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(s_layer, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_layer, LV_OBJ_FLAG_HIDDEN);

    // --- the news layout (hidden until card_set_news) ---
    s_art = lv_image_create(s_layer);
    lv_obj_align(s_art, LV_ALIGN_TOP_RIGHT, 0, NEWS_ART_Y);
    lv_obj_add_flag(s_art, LV_OBJ_FLAG_HIDDEN);

    s_chip = plain(s_layer);
    lv_obj_set_height(s_chip, 18);
    lv_obj_set_width(s_chip, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(s_chip, 9, 0);
    lv_obj_set_style_bg_opa(s_chip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_chip, 1, 0);
    lv_obj_set_style_pad_hor(s_chip, 8, 0);
    lv_obj_align(s_chip, LV_ALIGN_TOP_LEFT, NEWS_X, NEWS_CHIP_Y);
    lv_obj_add_flag(s_chip, LV_OBJ_FLAG_HIDDEN);
    s_chip_label = lv_label_create(s_chip);
    lv_obj_set_style_text_font(s_chip_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_letter_space(s_chip_label, 1, 0);
    lv_obj_center(s_chip_label);

    // The headline sits in a fixed window and is as tall as its text needs; if
    // that is more than the window shows, it drifts up to reveal the rest
    // (headline_scroll_start), pauses, and returns.
    s_head_box = plain(s_layer);
    lv_obj_set_size(s_head_box, NEWS_HEAD_W_ART, NEWS_HEAD_H);
    lv_obj_align(s_head_box, LV_ALIGN_TOP_LEFT, NEWS_X, NEWS_HEAD_Y);
    lv_obj_add_flag(s_head_box, LV_OBJ_FLAG_HIDDEN);
    s_headline = lv_label_create(s_head_box);
    lv_obj_set_width(s_headline, NEWS_HEAD_W_ART);
    lv_obj_set_height(s_headline, LV_SIZE_CONTENT);
    lv_label_set_long_mode(s_headline, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_font(s_headline, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_headline, lv_color_hex(0xf1f5fb), 0);
    lv_obj_align(s_headline, LV_ALIGN_TOP_LEFT, 0, 0);

    s_meta = lv_label_create(s_layer);
    lv_obj_set_style_text_font(s_meta, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_meta, lv_color_hex(0x7f8da0), 0);
    lv_obj_align(s_meta, LV_ALIGN_TOP_LEFT, NEWS_X, NEWS_META_Y);
    lv_obj_add_flag(s_meta, LV_OBJ_FLAG_HIDDEN);

    s_bubble = plain(s_layer);
    lv_obj_set_size(s_bubble, SIDE_W, SIDE_H);
    lv_obj_align(s_bubble, LV_ALIGN_TOP_LEFT, SIDE_X, SIDE_Y);
    lv_obj_set_style_radius(s_bubble, 14, 0);
    lv_obj_set_style_bg_color(s_bubble, lv_color_hex(0x101a2b), 0);
    lv_obj_set_style_bg_opa(s_bubble, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_bubble, 1, 0);
    lv_obj_set_style_border_color(s_bubble, lv_color_hex(0x25364f), 0);
    lv_obj_add_flag(s_bubble, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < PAGE_DOTS_MAX; i++) {
        s_dots[i] = plain(s_layer);
        lv_obj_set_size(s_dots[i], 6, 6);
        lv_obj_set_style_radius(s_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s_dots[i], LV_OPA_COVER, 0);
        lv_obj_add_flag(s_dots[i], LV_OBJ_FLAG_HIDDEN);
    }
    s_greet = lv_label_create(s_layer);
    lv_obj_set_style_text_font(s_greet, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_greet, lv_color_hex(0xc3cedd), 0);
    lv_obj_align(s_greet, LV_ALIGN_TOP_LEFT, NEWS_X, 62);
    lv_obj_add_flag(s_greet, LV_OBJ_FLAG_HIDDEN);

    s_header = lv_label_create(s_layer);
    lv_obj_set_style_text_color(s_header, lv_color_hex(theme->mouth), 0);
    lv_obj_set_style_text_letter_space(s_header, 2, 0);
    lv_obj_align(s_header, LV_ALIGN_TOP_LEFT, CARD_MARGIN_X, CARD_HEADER_Y);

    s_hint = lv_label_create(s_layer);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(theme->extra), 0);
    lv_label_set_text(s_hint, "");

    s_rule = plain(s_layer);
    lv_obj_set_size(s_rule, 28, 2);
    lv_obj_set_style_bg_color(s_rule, lv_color_hex(theme->mouth), 0);
    lv_obj_set_style_bg_opa(s_rule, LV_OPA_COVER, 0);
    lv_obj_align(s_rule, LV_ALIGN_TOP_LEFT, CARD_MARGIN_X, CARD_RULE_Y);

    s_text = lv_label_create(s_layer);
    lv_obj_set_size(s_text, CARD_TEXT_W, CARD_TEXT_H);
    lv_label_set_long_mode(s_text, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_font(s_text, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_text, lv_color_hex(theme->eye), 0);
    lv_obj_align(s_text, LV_ALIGN_TOP_LEFT, CARD_MARGIN_X, CARD_TEXT_Y);

    // Choice pills (hidden until a check-in asks for them).
    s_accent = theme->mouth;
    s_bg = theme->bg;
    s_dim = theme->extra;
    for (int i = 0; i < OPT_COUNT; i++) {
        s_opt[i] = plain(s_layer);
        lv_obj_set_size(s_opt[i], OPT_W, OPT_H);
        lv_obj_set_style_radius(s_opt[i], OPT_H / 2, 0);
        lv_obj_set_style_border_width(s_opt[i], 1, 0);
        lv_obj_set_style_bg_opa(s_opt[i], LV_OPA_COVER, 0);
        lv_obj_align(s_opt[i], LV_ALIGN_TOP_LEFT, CARD_MARGIN_X + i * (OPT_W + OPT_GAP), OPT_Y);
        lv_obj_add_flag(s_opt[i], LV_OBJ_FLAG_HIDDEN);

        s_opt_label[i] = lv_label_create(s_opt[i]);
        lv_obj_center(s_opt_label[i]);
    }
}

void card_set_options(const char *a, const char *b, const char *c)
{
    const char *texts[OPT_COUNT] = {a, b, c};
    for (int i = 0; i < OPT_COUNT; i++) {
        lv_label_set_text(s_opt_label[i], texts[i]);
        if (s_side) {
            lv_obj_set_size(s_opt[i], SOPT_W, SOPT_H);
            lv_obj_set_style_radius(s_opt[i], SOPT_H / 2, 0);
            lv_obj_align(s_opt[i], LV_ALIGN_TOP_LEFT, SIDE_TEXT_X + i * (SOPT_W + SOPT_GAP), SOPT_Y);
            lv_obj_set_style_text_font(s_opt_label[i], &lv_font_montserrat_12, 0);
        } else {
            lv_obj_set_size(s_opt[i], OPT_W, OPT_H);
            lv_obj_set_style_radius(s_opt[i], OPT_H / 2, 0);
            lv_obj_align(s_opt[i], LV_ALIGN_TOP_LEFT, CARD_MARGIN_X + i * (OPT_W + OPT_GAP), OPT_Y);
            lv_obj_set_style_text_font(s_opt_label[i], LV_FONT_DEFAULT, 0);
        }
        lv_obj_center(s_opt_label[i]);
        lv_obj_clear_flag(s_opt[i], LV_OBJ_FLAG_HIDDEN);
    }
    card_select(-1, 0.0f); // nothing chosen until the person tilts
}

void card_select(int index, float progress)
{
    if (progress < 0.0f) {
        progress = 0.0f;
    }
    if (progress > 1.0f) {
        progress = 1.0f;
    }
    for (int i = 0; i < OPT_COUNT; i++) {
        bool on = i == index;
        // The chosen pill's gold fill grows from a faint wash to solid as the
        // hold completes; the label flips to dark once it is mostly filled.
        uint8_t fill = on ? (uint8_t)(40.0f + progress * 215.0f) : 0;
        lv_obj_set_style_bg_color(s_opt[i], lv_color_hex(s_accent), 0);
        lv_obj_set_style_bg_opa(s_opt[i], fill, 0);
        lv_obj_set_style_border_color(s_opt[i], lv_color_hex(on ? s_accent : s_dim), 0);
        lv_obj_set_style_text_color(s_opt_label[i], lv_color_hex(on && progress > 0.5f ? s_bg : (on ? s_accent : s_dim)), 0);
    }
}

void card_set_hint(const char *hint)
{
    lv_label_set_text(s_hint, hint);
    if (s_side) {
        // In the bubble the hint takes the header's place until it is cleared.
        lv_obj_set_style_text_font(s_hint, &lv_font_montserrat_12, 0);
        lv_obj_align(s_hint, LV_ALIGN_TOP_LEFT, SIDE_TEXT_X, SIDE_Y + 11);
        show(s_hint, hint[0] != 0);
        show(s_header, hint[0] == 0);
        return;
    }
    lv_obj_align(s_hint, LV_ALIGN_TOP_RIGHT, -CARD_MARGIN_X, CARD_HEADER_Y + 2);
}

void card_clear_options(void)
{
    for (int i = 0; i < OPT_COUNT; i++) {
        lv_obj_add_flag(s_opt[i], LV_OBJ_FLAG_HIDDEN);
    }
    card_set_hint("");
}

static void show(lv_obj_t *o, bool on)
{
    if (on) {
        lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
}

// Which of the two layouts is showing. The simple one (small header, rule and
// large text) serves jokes, wellness lines and the check-in; the news one
// has the chip, headline, meta line and artwork.
static void use_layout(bool news)
{
    if (!news) {
        show(s_greet, false);
        card_set_pages(0, 0);
    }
    show(s_header, !news);
    show(s_rule, !news);
    show(s_text, !news);
    show(s_chip, news);
    show(s_head_box, news);
    if (!news) {
        headline_scroll_stop();
    }
    show(s_meta, news || s_side);
    show(s_bubble, !news && s_side);
    if (!news) {
        show(s_art, false);
    }
    if (news) {
        s_side = false;
        lv_obj_set_style_bg_opa(s_layer, LV_OPA_COVER, 0);
    }
}

void card_set_side(bool on)
{
    s_side = on;
    lv_obj_set_style_bg_opa(s_layer, on ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
}

void card_set_footer(const char *text)
{
    lv_label_set_text(s_meta, text ? text : "");
    lv_obj_set_style_text_font(s_meta, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_meta, lv_color_hex(s_accent), 0);
    lv_obj_align(s_meta, LV_ALIGN_TOP_RIGHT, -(320 - (SIDE_X + SIDE_W) + 12), SIDE_Y + 11); // right end of the header row
    lv_obj_move_foreground(s_meta); // above the bubble
    show(s_meta, s_side);
}

void card_set(const char *header, const char *text)
{
    use_layout(false);
    lv_label_set_text(s_header, header);
    lv_label_set_text(s_text, text);
    if (s_side) {
        // Short lines (a wellness nudge, a greeting) get the large font;
        // anything longer, such as a joke, the smaller one so it fits.
        bool big = strlen(text) <= 34;
        bool pills = !lv_obj_has_flag(s_opt[0], LV_OBJ_FLAG_HIDDEN);
        show(s_hint, false);
        show(s_header, true);
        show(s_rule, false);
        lv_obj_set_style_text_font(s_header, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_letter_space(s_header, 1, 0);
        lv_obj_align(s_header, LV_ALIGN_TOP_LEFT, SIDE_TEXT_X, SIDE_Y + 11);
        lv_obj_set_style_text_font(s_text, big ? &lv_font_montserrat_20 : &lv_font_montserrat_14, 0);
        lv_obj_set_size(s_text, SIDE_TEXT_W, pills ? 50 : (big ? 68 : 76));
        lv_obj_align(s_text, LV_ALIGN_TOP_LEFT, SIDE_TEXT_X, SIDE_Y + 30);
        lv_label_set_text(s_meta, "");
    } else {
        lv_obj_set_style_text_font(s_header, LV_FONT_DEFAULT, 0);
        lv_obj_set_size(s_text, CARD_TEXT_W, CARD_TEXT_H);
        lv_obj_set_style_text_font(s_text, &lv_font_montserrat_20, 0);
        lv_obj_align(s_text, LV_ALIGN_TOP_LEFT, CARD_MARGIN_X, CARD_TEXT_Y);
        lv_obj_align(s_header, LV_ALIGN_TOP_LEFT, CARD_MARGIN_X, CARD_HEADER_Y);
    }
}

// True if the file can be opened. Probed before an image is used, so a missing
// SD card (or a missing file) quietly means "no artwork" rather than LVGL
// drawing its "image not found" placeholder.
static bool file_exists(const char *path)
{
    lv_fs_file_t f;
    if (lv_fs_open(&f, path, LV_FS_MODE_RD) != LV_FS_RES_OK) {
        return false;
    }
    lv_fs_close(&f);
    return true;
}

void card_set_news(const char *category, uint32_t accent, const char *headline, const char *meta,
                   const char *art_path)
{
    use_layout(true);
    show(s_greet, false);
    s_news_accent = accent;
    lv_obj_set_style_text_font(s_headline, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_meta, lv_color_hex(0x7f8da0), 0);
    lv_obj_set_style_text_font(s_meta, &lv_font_montserrat_12, 0);
    lv_obj_align(s_head_box, LV_ALIGN_TOP_LEFT, NEWS_X, NEWS_HEAD_Y);
    lv_obj_align(s_meta, LV_ALIGN_TOP_LEFT, NEWS_X, NEWS_META_Y);
    card_set_pages(0, 0);

    // Chip: a dark wash of the category colour with a thin brighter outline
    // and the text in the colour itself - the same chip in each category's tint.
    lv_color_t a = lv_color_hex(accent);
    lv_obj_set_style_bg_color(s_chip, lv_color_mix(a, lv_color_hex(CARD_BG_TOP), 70), 0);
    lv_obj_set_style_border_color(s_chip, lv_color_mix(a, lv_color_hex(CARD_BG_TOP), 110), 0);
    lv_obj_set_style_text_color(s_chip_label, lv_color_mix(a, lv_color_hex(0xffffff), 200), 0);
    lv_label_set_text(s_chip_label, category);

    bool art = art_path && art_path[0] && file_exists(art_path);
    if (art) {
        lv_image_set_src(s_art, art_path);
        lv_obj_align(s_art, LV_ALIGN_TOP_RIGHT, 0, NEWS_ART_Y);
    }
    show(s_art, art);

    // With artwork the text keeps to the left column; without it, it may use
    // the full width.
    int head_w = art ? NEWS_HEAD_W_ART : NEWS_HEAD_W_FULL;
    lv_obj_set_size(s_head_box, head_w, NEWS_HEAD_H);
    lv_obj_set_width(s_headline, head_w);
    lv_label_set_text(s_headline, headline);
    lv_obj_update_layout(s_headline);
    s_scroll_overflow = (int)lv_obj_get_height(s_headline) - NEWS_HEAD_H;
    headline_scroll_start();
    lv_label_set_text(s_meta, meta ? meta : "");
}

void card_set_pages(int index, int count)
{
    if (count > PAGE_DOTS_MAX) {
        count = PAGE_DOTS_MAX;
    }
    for (int i = 0; i < PAGE_DOTS_MAX; i++) {
        bool on = count >= 2 && i < count;
        show(s_dots[i], on);
        if (!on) {
            continue;
        }
        lv_obj_align(s_dots[i], LV_ALIGN_TOP_RIGHT, -(NEWS_X + (count - 1 - i) * 12), 156);
        lv_obj_set_style_bg_color(s_dots[i], i == index ? lv_color_hex(s_news_accent) : lv_color_hex(0x3a4658), 0);
    }
}

void card_set_greeting(const char *hello, const char *name, const char *interests, const char *art_path)
{
    use_layout(true);
    show(s_chip, false);
    card_set_pages(0, 0);

    bool art = art_path && art_path[0] && file_exists(art_path);
    if (art) {
        lv_image_set_src(s_art, art_path);
        lv_obj_align(s_art, LV_ALIGN_TOP_LEFT, 0, 0);
    }
    show(s_art, art);

    lv_label_set_text(s_greet, hello ? hello : "");
    lv_obj_align(s_greet, LV_ALIGN_TOP_LEFT, NEWS_X, 56);
    show(s_greet, true);

    lv_obj_set_style_text_font(s_headline, &lv_font_montserrat_20, 0);
    s_scroll_overflow = 0;
    headline_scroll_stop();
    lv_obj_set_size(s_head_box, 288, 28);
    lv_obj_align(s_head_box, LV_ALIGN_TOP_LEFT, NEWS_X, 80);
    lv_obj_set_width(s_headline, 288);
    lv_label_set_text(s_headline, name ? name : "");

    lv_obj_set_style_text_color(s_meta, lv_color_hex(0xa3b0c2), 0);
    lv_label_set_text(s_meta, interests ? interests : "");
    lv_obj_align(s_meta, LV_ALIGN_TOP_LEFT, NEWS_X, 112);
}

void card_set_visibility(float alpha)
{
    if (alpha < 0.01f) {
        lv_obj_add_flag(s_layer, LV_OBJ_FLAG_HIDDEN);
        if (s_scroll_timer) {
            lv_timer_pause(s_scroll_timer); // nothing to do while hidden
        }
        return;
    }
    if (s_scroll_timer) {
        lv_timer_resume(s_scroll_timer);
    }
    lv_obj_clear_flag(s_layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(s_layer, (lv_opa_t)(alpha * 255.0f), 0);
}
