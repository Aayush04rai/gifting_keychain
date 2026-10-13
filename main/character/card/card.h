#pragma once

#include "lvgl.h"

#include "theme.h"

// The information card: a small category line and up to three lines of text
// (a headline or a joke) that temporarily takes the place of the face. It is
// deliberately plain - a gold label, one accent rule, large text on black -
// so it reads as the character handing you a note, not as a phone screen.
//
// The status strip is a sibling of this layer, not a child, so the clock and
// battery stay visible while a card is up.
void card_build(lv_obj_t *root, const char_theme_t *theme);

// The news layout: a category chip tinted with `accent` (0xRRGGBB), the headline
// (up to four lines, ending in "..." if longer), a small meta line such as
// "Reuters - 2h ago", and an artwork panel on the right drawn from `art_path`
// (an LVGL image path such as "S:ui/cat_ai.bin"). If the artwork cannot be
// found - no SD card, file missing - the panel is simply left out and the text
// takes the full width. `art_path` may be NULL.
void card_set_news(const char *category, uint32_t accent, const char *headline, const char *meta,
                   const char *art_path);

// Page dots at the bottom-right of the news card: `count` dots (hidden unless
// there are at least two), the `index`-th one lit in the card's tint.
void card_set_pages(int index, int count);

// The greeting card, shown on the home screen when there is no headline yet:
// "<hello>," in small type, the recipient's name large, an interests line
// beneath, and optional artwork behind (a full-card image, e.g.
// "S:ui/home.bin"; NULL or missing = plain gradient).
void card_set_greeting(const char *hello, const char *name, const char *interests, const char *art_path);

// Sets the two texts. `header` is shown as-is (pass upper case); `text` wraps
// over three lines and ends in "..." if it is longer.
void card_set(const char *header, const char *text);

// A row of three choices along the bottom of the card, for the daily
// check-in. Shown by card_set_options(), removed by card_clear_options()
// (every other card calls that first).
void card_set_options(const char *a, const char *b, const char *c);

// Highlights choice `index` (0..2), or none for -1. `progress` (0..1) is how
// far the hold-to-confirm has got: the chosen pill fills with gold from
// faint to solid as it approaches 1, so the person can see an answer being
// given - and tilt away to take it back.
void card_select(int index, float progress);
void card_clear_options(void);

// A small dim line at the right of the header, e.g. "tilt to answer". Pass
// "" to remove it.
void card_set_hint(const char *hint);

// The "talking" layout used by jokes, wellness lines and the check-in: the
// card has no background of its own (the face stays on screen, shrunk to the
// left - see char_layout_t) and its text sits in a rounded bubble on the right.
// Turn it on before card_set(); news cards turn it off again.
void card_set_side(bool on);

// A small line at the right end of the side bubble's header row (e.g. exercise rep dots).
void card_set_footer(const char *text);

// 0 hides the card completely (and stops drawing it); 1 is fully shown.
void card_set_visibility(float alpha);
