#include <stdio.h>
#include <string.h>
#include <time.h>

#include "board_hal.h"
#include "status.h"

// Layout, in px from the strip's top-right corner. Right to left: battery,
// its percentage, then the Wi-Fi icon. The clock sits at top-left.
#define BATT_W        22
#define BATT_H        11
#define BATT_RIGHT    12   // gap from the right edge to the body
#define BATT_TOP      5
#define PCT_RIGHT     38
#define WIFI_CX       92   // icon centre, measured from the right edge
#define WIFI_CY       16   // the dot and the arcs' common centre, from the top
#define WIFI_DOT_D    4
#define WIFI_ARCS     3

#define SAMPLE_MS     5000 // battery and signal change slowly; no need to poll faster

#define COLOR_DIM     0x2a333d
#define COLOR_LOW     0xe3a857  // amber: battery 15% or less
#define COLOR_CRIT    0xd96459  // red: battery 8% or less
#define COLOR_CHARGE  0x4ecb8f  // green: on charge

static lv_obj_t *s_time;
static lv_obj_t *s_pct;
static lv_obj_t *s_batt_body;
static lv_obj_t *s_batt_fill;
static lv_obj_t *s_batt_nub;
static lv_obj_t *s_wifi_dot;
static lv_obj_t *s_wifi_arc[WIFI_ARCS];

static uint32_t s_normal_color;
static uint32_t s_next_sample;
static char s_shown_time[8];
static int s_shown_pct = -2;   // -2 = nothing drawn yet
static bool s_shown_charging;
static int s_shown_bars = -1;

static lv_obj_t *plain(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

void status_build(lv_obj_t *root, const char_theme_t *theme)
{
    s_normal_color = theme->extra;

    s_time = lv_label_create(root);
    lv_obj_set_style_text_color(s_time, lv_color_hex(s_normal_color), 0);
    lv_obj_align(s_time, LV_ALIGN_TOP_LEFT, 10, 4);

    // Battery: outlined body, a fill that shrinks with charge, and the
    // little terminal nub on the right.
    s_batt_body = plain(root);
    lv_obj_set_size(s_batt_body, BATT_W, BATT_H);
    lv_obj_set_style_radius(s_batt_body, 3, 0);
    lv_obj_set_style_border_width(s_batt_body, 1, 0);
    lv_obj_set_style_border_color(s_batt_body, lv_color_hex(s_normal_color), 0);
    lv_obj_set_style_pad_all(s_batt_body, 2, 0);
    lv_obj_align(s_batt_body, LV_ALIGN_TOP_RIGHT, -BATT_RIGHT, BATT_TOP);

    s_batt_fill = plain(s_batt_body);
    lv_obj_set_style_radius(s_batt_fill, 1, 0);
    lv_obj_set_style_bg_opa(s_batt_fill, LV_OPA_COVER, 0);
    lv_obj_align(s_batt_fill, LV_ALIGN_LEFT_MID, 0, 0);

    s_batt_nub = plain(root);
    lv_obj_set_size(s_batt_nub, 2, 5);
    lv_obj_set_style_radius(s_batt_nub, 1, 0);
    lv_obj_set_style_bg_opa(s_batt_nub, LV_OPA_COVER, 0);
    lv_obj_align(s_batt_nub, LV_ALIGN_TOP_RIGHT, -(BATT_RIGHT - 3), BATT_TOP + 3);

    s_pct = lv_label_create(root);
    lv_obj_set_style_text_color(s_pct, lv_color_hex(s_normal_color), 0);
    lv_obj_align(s_pct, LV_ALIGN_TOP_RIGHT, -PCT_RIGHT, 3);

    // Wi-Fi: the familiar fan - a dot with three arcs growing out of it.
    // Each arc is a concentric circle of which only the top 90 degrees is
    // drawn (0 deg is 3 o'clock, angles run clockwise, so 225-315 is the
    // wedge pointing straight up).
    s_wifi_dot = plain(root);
    lv_obj_set_size(s_wifi_dot, WIFI_DOT_D, WIFI_DOT_D);
    lv_obj_set_style_radius(s_wifi_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_wifi_dot, LV_OPA_COVER, 0);
    lv_obj_align(s_wifi_dot, LV_ALIGN_TOP_RIGHT, -(WIFI_CX - WIFI_DOT_D / 2), WIFI_CY - WIFI_DOT_D / 2);

    static const int k_arc_d[WIFI_ARCS] = {10, 17, 24};
    for (int i = 0; i < WIFI_ARCS; i++) {
        int d = k_arc_d[i];
        lv_obj_t *arc = lv_arc_create(root);
        lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
        lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(arc, d, d);
        lv_arc_set_rotation(arc, 0);
        lv_arc_set_bg_angles(arc, 225, 315);
        lv_obj_set_style_arc_width(arc, 2, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
        lv_obj_align(arc, LV_ALIGN_TOP_RIGHT, -(WIFI_CX - d / 2), WIFI_CY - d / 2);
        s_wifi_arc[i] = arc;
    }

    s_shown_time[0] = '\0';
    s_shown_pct = -2;
    s_shown_bars = -1;
    s_next_sample = 0;
    status_apply(0);
}

// Minimum dBm for 4, 3 and 2 bars; anything weaker but connected is 1.
static const int k_bar_floor[] = {-55, -65, -75};
#define BAR_HYSTERESIS_DB 4

// `shown` is the bar count currently drawn. Moving to a better count needs
// the signal to clear the threshold by BAR_HYSTERESIS_DB, so a reading that
// hovers right on a boundary (-55 / -56 / -54 ...) doesn't flicker the bars.
static int bars_from_rssi(int rssi, int shown)
{
    if (rssi == 0) {
        return 0; // not connected
    }
    int bars = 1;
    for (int i = 0; i < 3; i++) {
        if (rssi >= k_bar_floor[i]) {
            bars = 4 - i;
            break;
        }
    }
    if (shown >= 1 && bars > shown && shown <= 3) {
        // Wants to go up: only if it clears the next threshold with margin.
        if (rssi < k_bar_floor[4 - shown - 1] + BAR_HYSTERESIS_DB) {
            return shown;
        }
    } else if (shown >= 1 && bars < shown) {
        // Wants to go down: only once it is clearly below the current one.
        if (rssi >= k_bar_floor[4 - shown] - BAR_HYSTERESIS_DB) {
            return shown;
        }
    }
    return bars;
}

static void draw_battery(int pct, bool charging)
{
    if (pct < 0) {
        // No battery to measure: show nothing rather than a misleading 0%.
        lv_obj_add_flag(s_batt_body, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_batt_nub, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_pct, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s_batt_body, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_batt_nub, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_pct, LV_OBJ_FLAG_HIDDEN);

    // Green while charging (the low-battery colours do not apply: it is
    // already being fed); otherwise the usual, amber and red as it runs down.
    uint32_t color = s_normal_color;
    if (charging) {
        color = COLOR_CHARGE;
    } else if (pct <= 8) {
        color = COLOR_CRIT;
    } else if (pct <= 15) {
        color = COLOR_LOW;
    }

    // Inner width of the body is BATT_W - 2*border - 2*padding = 14 px.
    int fill_w = pct * 14 / 100;
    if (pct > 0 && fill_w < 1) {
        fill_w = 1;
    }
    lv_obj_set_size(s_batt_fill, fill_w, BATT_H - 6);
    lv_obj_set_style_bg_color(s_batt_fill, lv_color_hex(color), 0);
    lv_obj_set_style_border_color(s_batt_body, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(s_batt_nub, lv_color_hex(color), 0);

    char buf[12];
    snprintf(buf, sizeof(buf), "%d%%", pct > 100 ? 100 : pct);
    lv_label_set_text(s_pct, buf);
    lv_obj_set_style_text_color(s_pct, lv_color_hex(color), 0);
    lv_obj_align(s_pct, LV_ALIGN_TOP_RIGHT, -PCT_RIGHT, 3);
}

// `level` is 0 (not connected) .. 4 (strong). The dot lights as soon as the
// board is connected, then one more arc per level: 1 = dot only (barely
// there), 4 = dot and all three arcs. Unlit parts stay visible but dim, so
// the icon's shape is always readable, including when offline.
static void draw_wifi(int level)
{
    lv_obj_set_style_bg_color(s_wifi_dot, lv_color_hex(level >= 1 ? s_normal_color : COLOR_DIM), 0);
    for (int i = 0; i < WIFI_ARCS; i++) {
        uint32_t color = (level - 1) > i ? s_normal_color : COLOR_DIM;
        lv_obj_set_style_arc_color(s_wifi_arc[i], lv_color_hex(color), LV_PART_MAIN);
    }
}

static void sample_hardware(void)
{
    int pct = board_get_battery_pct();
    bool charging = pct >= 0 && board_battery_charging();
    // The ADC wobbles by a point or two; only redraw for a real change (or
    // when the battery appears/disappears, or starts/stops charging) so the
    // number doesn't flicker.
    bool presence_changed = (pct < 0) != (s_shown_pct < 0) || s_shown_pct == -2;
    if (presence_changed || charging != s_shown_charging || (pct >= 0 && (pct - s_shown_pct >= 2 || s_shown_pct - pct >= 2))) {
        s_shown_pct = pct < 0 ? -1 : pct;
        s_shown_charging = charging;
        draw_battery(pct, charging);
    }

    int rssi = board_get_wifi_rssi();
    int bars = bars_from_rssi(rssi, s_shown_bars);
    if (bars != s_shown_bars) {
        s_shown_bars = bars;
        draw_wifi(bars);
        printf("[status] wifi rssi=%d dBm -> %d bars, battery=%d%%\n", rssi, bars, pct);
    }
}

void status_apply(uint32_t now_ms)
{
    if (s_next_sample == 0 || now_ms >= s_next_sample) {
        sample_hardware();
        s_next_sample = now_ms + SAMPLE_MS;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    // Before the first SNTP sync the clock reads 1970 (counting from boot);
    // show a placeholder instead of a wrong time, and the real time appears
    // on the first tick after the sync lands.
    char buf[8];
    if (!tm_info || tm_info->tm_year + 1900 < 2024) {
        snprintf(buf, sizeof(buf), "--:--");
    } else {
        snprintf(buf, sizeof(buf), "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
    }

    // Only touch the label when the text changes - this runs at 30 fps.
    if (strcmp(buf, s_shown_time) != 0) {
        snprintf(s_shown_time, sizeof(s_shown_time), "%s", buf);
        lv_label_set_text(s_time, buf);
    }
}
