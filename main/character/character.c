#include <math.h>
#include <stdio.h>
#include <string.h>

#include "acts/acts.h"
#include "board_hal.h"
#include "card/card.h"
#include "character.h"
#include "expressions.h"
#include "extras/extras.h"
#include "eyes/eyes.h"
#include "led/led.h"
#include "mouth/mouth.h"
#include "onboarding.h"
#include "power.h"
#include "status/status.h"
#include "theme.h"
#include "wellness.h"

#define TICK_MS 33

// Blink: ~110 ms to close, ~140 ms to open, every 2.8-6.5 s. The long gaps
// matter as much as the blink - stillness is what makes it feel alive.
#define BLINK_CLOSE_MS 110
#define BLINK_OPEN_MS  140

// Idle glances: not more than one every 3.5-8 s, held ~1 s, then back to
// centre. A rare mood flash (a moment of curiosity or a small smile) comes
// roughly every 25-50 s.
#define GLANCE_MIN_MS 3500
#define GLANCE_MAX_MS 8000
#define MOOD_MIN_MS   25000
#define MOOD_MAX_MS   50000
#define MOOD_HOLD_MS  1800

#define SHOWCASE_STEP_MS 3500

// Active -> idle -> sleepy -> asleep, by how long the device has been still.
// A change in acceleration smaller than MOTION_G between polls is sensor
// noise / the desk humming, not someone picking it up.
#define MOTION_G         0.06f
#define SLEEPY_AFTER_MS  180000
#define SLEEP_AFTER_MS   270000

// Left alone and un-noticed: the first act comes after this long (and the
// companion is idle, nothing else going on), then another every 50-90 s until
// it gets sleepy. Being picked up or nudged resets the clock.
#define NEGLECT_FIRST_MS   60000
#define NEGLECT_GAP_MIN_MS 50000
#define NEGLECT_GAP_MAX_MS 90000
// Fully asleep this long, with nothing happening: go deep (screen off, chip in
// light sleep). About 7 minutes of stillness in all.
#define DEEP_AFTER_MS    180000
// While deep, the engine ticks only a couple of times a second.
#define DEEP_TICK_MS     500
// Backlight (%) for the drowsy stages; awake brightness is
// CHAR_BACKLIGHT_AWAKE_PCT in character.h.
#define BACKLIGHT_SLEEPY_PCT    40
#define BACKLIGHT_SLEEPING_PCT  12

static const char_theme_t *s_theme = &CHAR_THEME_EXECUTIVE;

static void event_cancel(void);
static void exit_deep(void);

static uint32_t s_sleepy_after = SLEEPY_AFTER_MS;
static uint32_t s_sleep_after = SLEEP_AFTER_MS;
static uint32_t s_last_motion;
static float s_led_boost;

// Two modes. HOME is the product's front face: an information card (the
// recipient's interests), refreshed by events.c every few minutes, with the
// character out of sight. After SAVER_AFTER_MS without any movement the device
// becomes a SCREENSAVER: the character appears and does its idle things
// (glances, sleepiness, left-alone acts). Picking it up returns to HOME.
// Jokes, wellness moments and the check-in still bring the character out
// briefly, over either mode.
#define SAVER_AFTER_MS (5u * 60 * 1000)
#define HOME_TICK_MS   250 // the engine barely needs to run while a static card is up
static uint32_t s_saver_after = SAVER_AFTER_MS;
static bool s_saver;
static uint32_t s_saver_since;
static bool s_exit_saver_after_wake;
static bool s_demo_saver; // demo: left-alone acts follow each other closely
static uint32_t s_tick_ms = TICK_MS;
static void tick_fast(void);
static void find_motion_hook(uint32_t now);

static char_params_t s_cur;
static char_emotion_t s_emotion;
static float s_look_x, s_look_y;           // live (smoothed) gaze
static float s_glance_x, s_glance_y;       // idle glance target
static float s_override_x, s_override_y;   // caller-driven gaze
static uint32_t s_override_until;

static uint32_t s_blink_start;
static uint32_t s_next_blink;
static uint32_t s_next_glance;
static uint32_t s_glance_end;
static uint32_t s_next_mood;

// Focus / do-not-disturb (see character_toggle_focus in character.h). Declared
// up here because the idle behaviour below is calmer while it is on.
static bool s_focus;
static uint32_t s_focus_until;
static float s_focus_alpha;

// Left-alone acts (acts/acts.h). Declared this early because motion, shakes
// and events all need to be able to cut one short.
static bool s_act_running;
static uint32_t s_next_act;
static uint32_t s_neglect_first = NEGLECT_FIRST_MS;
static uint32_t s_mood_end;
static bool s_mood_active;

#if CHARACTER_SHOWCASE
static uint32_t s_showcase_next;
static uint32_t s_showcase_index;
#endif

// Simulator-only (defined in sim/CMakeLists.txt): CHAR_EMOTION=<0..6> pins the
// character to one emotion with no blinking or glancing, so each expression
// can be inspected or screenshotted in a known, settled state.
#ifdef CHARACTER_DEBUG_ENV
#include <stdlib.h>
static bool s_freeze;
#else
#define s_freeze false
#endif

static uint32_t rand_between(uint32_t lo, uint32_t hi)
{
    return lo + lv_rand(0, hi - lo);
}

static float rand_signed(float magnitude)
{
    return ((float)lv_rand(0, 2000) / 1000.0f - 1.0f) * magnitude;
}

static void tween(float *cur, float target, float k)
{
    *cur += (target - *cur) * k;
}

static void tween_params(char_params_t *cur, const char_params_t *tgt, float k)
{
    // The params struct is all floats, so it can be walked as an array; keeps
    // adding a parameter to one edit in params.h + the expression table.
    float *c = (float *)cur;
    const float *t = (const float *)tgt;
    for (size_t i = 0; i < sizeof(char_params_t) / sizeof(float); i++) {
        tween(&c[i], t[i], k);
    }
}

// 0 open .. 1 shut for the current moment of a blink, 0 if none in progress.
static float blink_amount(uint32_t now)
{
    if (s_blink_start == 0) {
        return 0.0f;
    }
    uint32_t dt = now - s_blink_start;
    if (dt < BLINK_CLOSE_MS) {
        return (float)dt / BLINK_CLOSE_MS;
    }
    if (dt < BLINK_CLOSE_MS + BLINK_OPEN_MS) {
        return 1.0f - (float)(dt - BLINK_CLOSE_MS) / BLINK_OPEN_MS;
    }
    s_blink_start = 0;
    return 0.0f;
}

// Blinking while the eyes are already (almost) shut would just be noise.
static bool can_blink(void)
{
    return s_emotion != CHAR_EMOTION_SLEEPING && s_cur.eye_h > 0.6f;
}

static void run_idle_behaviour(uint32_t now)
{
    if (can_blink() && s_blink_start == 0 && now >= s_next_blink) {
        s_blink_start = now ? now : 1;
        s_next_blink = now + rand_between(2800, 6500);
        // Occasionally a quick double blink.
        if (lv_rand(0, 9) == 0) {
            s_next_blink = now + BLINK_CLOSE_MS + BLINK_OPEN_MS + 90;
        }
    }

    bool ambient = s_emotion == CHAR_EMOTION_IDLE;

    if (ambient && now >= s_next_glance) {
        // Mostly sideways; looking up is the least frequent.
        s_glance_x = rand_signed(0.9f);
        s_glance_y = lv_rand(0, 3) == 0 ? -0.6f : rand_signed(0.25f);
        s_glance_end = now + rand_between(700, 1500);
        // In focus mode the character is calmer: a third as many glances.
        s_next_glance = s_glance_end + rand_between(GLANCE_MIN_MS, GLANCE_MAX_MS) * (s_focus ? 3 : 1);
    }
    if (now >= s_glance_end) {
        s_glance_x = 0.0f;
        s_glance_y = 0.0f;
    }

    // A short unprompted mood, then back to neutral.
    if (ambient && !s_focus && !s_mood_active && now >= s_next_mood) {
        character_set_emotion(lv_rand(0, 1) ? CHAR_EMOTION_CURIOUS : CHAR_EMOTION_HAPPY);
        s_mood_active = true;
        s_mood_end = now + MOOD_HOLD_MS;
    } else if (s_mood_active && now >= s_mood_end) {
        s_mood_active = false;
        s_next_mood = now + rand_between(MOOD_MIN_MS, MOOD_MAX_MS);
        character_set_emotion(CHAR_EMOTION_IDLE);
    }
}

// --- Shake reaction: surprised -> dizzy -> recover -> idle ---------------

typedef enum { SHAKE_NONE = 0, SHAKE_SURPRISED, SHAKE_DIZZY, SHAKE_RECOVER } shake_stage_t;

#define SHAKE_SURPRISED_MS 650
#define SHAKE_DIZZY_MS     2600
#define SHAKE_RECOVER_MS   700

static shake_stage_t s_shake_stage;
static uint32_t s_shake_next;
static bool s_shake_pending;

void character_on_shake(void)
{
    tick_fast();
    exit_deep(); // a shake wakes a deeply sleeping device just as movement does
    // Another jerk while already dizzy just keeps it going a little longer.
    if (s_shake_stage == SHAKE_DIZZY) {
        s_shake_next = lv_tick_get() + SHAKE_DIZZY_MS;
        return;
    }
    if (s_shake_stage == SHAKE_NONE) {
        s_shake_pending = true;
    }
}

static void run_shake_sequence(uint32_t now)
{
    if (s_shake_pending) {
        s_shake_pending = false;
        s_shake_stage = SHAKE_SURPRISED;
        s_shake_next = now + SHAKE_SURPRISED_MS;
        s_mood_active = false;
        event_cancel(); // a shake interrupts a card; the card fades out on its own
        acts_cancel();
        s_act_running = false;
        s_blink_start = 0;
        s_glance_x = s_glance_y = 0.0f;
        s_glance_end = 0;
        character_set_emotion(CHAR_EMOTION_SURPRISED);
        return;
    }

    if (s_shake_stage == SHAKE_NONE || now < s_shake_next) {
        return;
    }

    switch (s_shake_stage) {
    case SHAKE_SURPRISED:
        s_shake_stage = SHAKE_DIZZY;
        s_shake_next = now + SHAKE_DIZZY_MS;
        character_set_emotion(CHAR_EMOTION_DIZZY);
        break;
    case SHAKE_DIZZY:
        // Recover: settle the face to neutral, then a deliberate slow blink.
        s_shake_stage = SHAKE_RECOVER;
        s_shake_next = now + SHAKE_RECOVER_MS;
        character_set_emotion(CHAR_EMOTION_IDLE);
        s_blink_start = now ? now : 1;
        break;
    default:
        s_shake_stage = SHAKE_NONE;
        s_next_glance = now + rand_between(GLANCE_MIN_MS, GLANCE_MAX_MS);
        s_next_blink = now + rand_between(2800, 6500);
        s_next_mood = now + rand_between(MOOD_MIN_MS, MOOD_MAX_MS);
        break;
    }
}

// --- Sleep and wake -------------------------------------------------------
//
// Wake-up: one eye cracks open, then both, a look left and right, a small
// smile, then neutral. Deliberately unhurried - the most polished moment in
// the product.

typedef enum { WAKE_NONE = 0, WAKE_EYE1, WAKE_EYE2, WAKE_LOOK, WAKE_SMILE } wake_stage_t;

#define WAKE_EYE1_MS  750
#define WAKE_EYE2_MS  650
#define WAKE_LOOK_MS  1300
#define WAKE_SMILE_MS 1100

static wake_stage_t s_wake_stage;
static uint32_t s_wake_next;
static uint32_t s_wake_stage_start;
static bool s_wake_looked_right;
static bool s_wake_pending;

// --- Screen brightness and deep sleep ---------------------------------------
//
// The screen dims as the character gets drowsy (the backlight is most of the
// device's power draw), and after a few minutes fully asleep it goes deep:
// screen off, chip in light sleep (power.h). Any movement undoes it at once.

static bool s_deep;
static uint32_t s_sleep_since;
static uint32_t s_deep_after = DEEP_AFTER_MS;
static float s_bl_cur = CHAR_BACKLIGHT_AWAKE_PCT;
static int s_bl_shown = CHAR_BACKLIGHT_AWAKE_PCT;
static lv_timer_t *s_tick_timer;

bool character_is_deep(void)
{
    return s_deep;
}

static void enter_deep(void)
{
    s_deep = true;
    led_off();
    power_deep_enter();
    // Nothing is on screen, so the engine only needs to keep an ear out.
    if (s_tick_timer) {
        lv_timer_set_period(s_tick_timer, DEEP_TICK_MS);
        s_tick_ms = DEEP_TICK_MS;
    }
}

static void exit_deep(void)
{
    if (!s_deep) {
        return;
    }
    s_deep = false;
    power_deep_exit();
    if (s_tick_timer) {
        lv_timer_set_period(s_tick_timer, TICK_MS);
        s_tick_ms = TICK_MS;
    }
    // The panel is awake but the backlight is still off: let the brightness
    // logic fade it up from zero rather than snapping it on.
    s_bl_cur = 0.0f;
    s_bl_shown = 0;
    s_sleep_since = 0;
}

// Eases the backlight toward what the current state calls for.
static void update_backlight(void)
{
    float target = CHAR_BACKLIGHT_AWAKE_PCT;
    if (s_emotion == CHAR_EMOTION_SLEEPING) {
        target = BACKLIGHT_SLEEPING_PCT;
    } else if (s_emotion == CHAR_EMOTION_SLEEPY) {
        target = BACKLIGHT_SLEEPY_PCT;
    }
    s_bl_cur += (target - s_bl_cur) * 0.06f;
    int pct = (int)(s_bl_cur + 0.5f);
    if (pct != s_bl_shown) {
        s_bl_shown = pct;
        board_set_backlight((uint8_t)pct);
    }
}

static bool event_idle(void); // defined with the event state machine

static void leave_saver(uint32_t now)
{
    (void)now;
    s_saver = false;
    s_demo_saver = false;
    s_mood_active = false;
    if (event_idle() && s_shake_stage == SHAKE_NONE && s_wake_stage == WAKE_NONE) {
        character_set_emotion(CHAR_EMOTION_IDLE);
    }
}

static void enter_saver(uint32_t now)
{
    s_saver = true;
    s_saver_since = now;
    s_next_act = now + s_neglect_first;
    s_mood_active = false;
    s_glance_x = s_glance_y = 0.0f;
    s_glance_end = 0;
    s_next_glance = now + rand_between(GLANCE_MIN_MS, GLANCE_MAX_MS);
    s_next_blink = now + rand_between(1500, 3500);
    s_next_mood = now + rand_between(MOOD_MIN_MS, MOOD_MAX_MS);
    character_set_emotion(CHAR_EMOTION_IDLE);
}

static void tick_fast(void)
{
    if (s_tick_timer && s_tick_ms != TICK_MS && !s_deep) {
        s_tick_ms = TICK_MS;
        lv_timer_set_period(s_tick_timer, TICK_MS);
    }
}

bool character_demo_saver(uint32_t act_delay_ms)
{
    if (!character_ready_for_event()) {
        return false;
    }
    uint32_t now = lv_tick_get();
    tick_fast();
    enter_saver(now);
    s_next_act = now + act_delay_ms;
    s_demo_saver = true;
    return true;
}

void character_motion(float delta_g)
{
    uint32_t now = lv_tick_get();
    if (delta_g < MOTION_G) {
        return;
    }
    s_last_motion = now;
    s_next_act = now + s_neglect_first; // noticed: the "left alone" clock starts again
    find_motion_hook(now);

    if (!s_saver) {
        return; // on the home card movement is just "someone is here"
    }

    // Picked up while deeply asleep: wake the screen immediately so the
    // wake-up animation is seen from its first frame.
    exit_deep();

    if (s_act_running) {
        acts_cancel();
        s_act_running = false;
    }
    if (s_emotion == CHAR_EMOTION_SLEEPING && s_wake_stage == WAKE_NONE) {
        // The character wakes up first (the full routine), then the home card returns.
        s_wake_pending = true;
        s_exit_saver_after_wake = true;
    } else if (s_wake_stage == WAKE_NONE) {
        leave_saver(now);
    }
}

static void run_wake_sequence(uint32_t now)
{
    // A shake outranks waking up: it goes straight to surprised.
    if (s_shake_stage != SHAKE_NONE) {
        s_wake_stage = WAKE_NONE;
        s_wake_pending = false;
        return;
    }

    if (s_wake_pending) {
        s_wake_pending = false;
        s_wake_stage = WAKE_EYE1;
        s_wake_stage_start = now;
        s_wake_next = now + WAKE_EYE1_MS;
        s_blink_start = 0;
        s_glance_x = s_glance_y = 0.0f;
        s_glance_end = 0;
        character_set_emotion(CHAR_EMOTION_WAKING1);
        return;
    }

    if (s_wake_stage == WAKE_NONE) {
        return;
    }

    if (s_wake_stage == WAKE_LOOK && !s_wake_looked_right && now >= s_wake_stage_start + 600) {
        s_wake_looked_right = true;
        character_look(0.9f, 0.0f, 550);
    }

    if (now < s_wake_next) {
        return;
    }

    s_wake_stage_start = now;
    switch (s_wake_stage) {
    case WAKE_EYE1:
        s_wake_stage = WAKE_EYE2;
        s_wake_next = now + WAKE_EYE2_MS;
        s_led_boost = 0.9f; // the bright pulse that fades to normal
        character_set_emotion(CHAR_EMOTION_WAKING2);
        break;
    case WAKE_EYE2:
        s_wake_stage = WAKE_LOOK;
        s_wake_next = now + WAKE_LOOK_MS;
        s_wake_looked_right = false;
        character_look(-0.9f, 0.0f, 550);
        break;
    case WAKE_LOOK:
        s_wake_stage = WAKE_SMILE;
        s_wake_next = now + WAKE_SMILE_MS;
        character_set_emotion(CHAR_EMOTION_HAPPY);
        break;
    default:
        s_wake_stage = WAKE_NONE;
        character_set_emotion(CHAR_EMOTION_IDLE);
        if (s_exit_saver_after_wake) {
            s_exit_saver_after_wake = false;
            leave_saver(now);
        }
        s_last_motion = now;
        s_next_glance = now + rand_between(GLANCE_MIN_MS, GLANCE_MAX_MS);
        s_next_blink = now + rand_between(2800, 6500);
        s_next_mood = now + rand_between(MOOD_MIN_MS, MOOD_MAX_MS);
        break;
    }
}

static void run_sleep_schedule(uint32_t now)
{
    uint32_t still = now - s_saver_since; // drowsiness counts from when the screensaver began

    if (s_emotion == CHAR_EMOTION_IDLE && still > s_sleepy_after) {
        s_mood_active = false;
        character_set_emotion(CHAR_EMOTION_SLEEPY);
    } else if (s_emotion == CHAR_EMOTION_SLEEPY && still > s_sleep_after) {
        character_set_emotion(CHAR_EMOTION_SLEEPING);
    }
}

// --- Information cards and wellness moments --------------------------------
//
// One state machine drives every "the character has something for you"
// moment:
//
//   news / joke / wellness text:  notice -> card -> return
//   breathe (wordless):           calm -> eyes closed, breathing -> return
//   check-in:                     notice -> question card (wait for an
//                                 answer) -> reply card -> return
//                                 ("Rough" adds a shared breath first)
//
// The face and the card are two layers cross-faded by s_card_alpha, so the
// switch is a dissolve rather than a cut.

typedef enum {
    EVT_NONE = 0,
    EVT_NOTICE,
    EVT_CARD,
    EVT_BREATHE,
    EVT_ASK,
    EVT_REPLY,
    EVT_RETURN,
} evt_stage_t;

// What kind of moment is in progress. The card kinds come from the public
// enum; these two have no card of their own.
#define EVT_KIND_BREATHE 100
#define EVT_KIND_CHECKIN 101
#define EVT_KIND_LOWBATT 102
#define EVT_KIND_FIND    103
#define EVT_KIND_PAIR    104

// How long the exhausted face stays up: long enough to be understood, short
// enough not to become a nag.
#define EVT_LOWBATT_MS 6500

#define EVT_NOTICE_NEWS_MS 900
#define EVT_NOTICE_JOKE_MS 1100
#define EVT_NOTICE_CALM_MS 1000
#define EVT_RETURN_MS      450
#define EVT_BREATHE_MS     10500
#define EVT_REPLY_MS       3800
#define EVT_ASK_TIMEOUT_MS 20000
#define BREATH_PERIOD_MS   5000

static evt_stage_t s_evt_stage;
static uint32_t s_evt_next;
static bool s_evt_pending;
static int s_evt_kind;
static char s_evt_header[24];
static char s_evt_text[112];
// Extra fields for a news card (character_show_news).
static bool s_evt_news;
static char s_evt_meta[48];
static char s_evt_art[40];
static uint32_t s_evt_accent;
static float s_card_alpha;

// Jokes, wellness lines and the check-in "talk to you": the face shrinks to the
// left of the screen (this layout, tweened) and the card's text sits in a bubble
// on the right. News keeps the full-screen card.
#define SIDE_SCALE   0.60f
#define SIDE_OX      (-89.0f)
#define SIDE_OY      8.0f
#define EX_REP_MS    2000
#define EX_REPS      3
static char_layout_t s_layout = {1.0f, 0.0f, 0.0f};
static uint32_t s_card_t0;
static uint32_t s_card_hold;
static bool s_active;
static lv_obj_t *s_face_layer;
static int s_choice;            // check-in: 0 Good, 1 Okay, 2 Rough
static bool s_reply_breathe;    // after a "Rough" reply, breathe with them

#define FOCUS_MAX_MS (90u * 60 * 1000)

// Long enough to read comfortably, short enough not to hold the device
// hostage: a base time plus a little per character, clamped.
static uint32_t card_hold_ms(int kind, size_t len)
{
    bool news = kind == CHAR_CARD_NEWS;
    uint32_t ms = news ? 3500 + 55 * len : 3000 + 60 * len;
    uint32_t lo = news ? 6000 : 5000;
    uint32_t hi = news ? 10000 : 8500;
    return ms < lo ? lo : (ms > hi ? hi : ms);
}

static bool evt_kind_is_side(void)
{
    return s_evt_kind == CHAR_CARD_JOKE || s_evt_kind == CHAR_CARD_WELLNESS || s_evt_kind == EVT_KIND_CHECKIN ||
           s_evt_kind == EVT_KIND_FIND || s_evt_kind == EVT_KIND_PAIR;
}

// Moving between the face and a card is done as a cut hidden behind a blink,
// not a fade: fading a whole layer makes LVGL redraw its entire subtree in
// small chunks every frame, which is what made transitions lag. A blink only
// moves two eyelids, and the cut happens while they are shut.
//
// The card is up from the start of its stage until the eyes have shut again
// at the end of it (the first BLINK_CLOSE_MS of the RETURN stage).
static bool card_up(uint32_t now)
{
    if (s_evt_stage == EVT_CARD || s_evt_stage == EVT_ASK || s_evt_stage == EVT_REPLY) {
        return true;
    }
    return s_evt_stage == EVT_RETURN && now < s_evt_next - EVT_RETURN_MS + BLINK_CLOSE_MS;
}

// True while a side-layout card is up.
static bool side_card_showing(uint32_t now)
{
    return evt_kind_is_side() && card_up(now);
}

static bool event_idle(void)
{
    return s_evt_stage == EVT_NONE;
}

// --- The home card ----------------------------------------------------------

typedef struct {
    int kind; // 0 nothing yet, 1 headline, 2 greeting
    char category[24];
    uint32_t accent;
    char head[112];  // headline, or the name
    char meta[48];   // "Source - 2h ago", or the interests line
    char art[40];
    char hello[24];  // greeting only
    int page, pages;
} home_t;
static home_t s_home;
static bool s_home_applied;

static void apply_home(void)
{
    card_clear_options();
    card_set_side(false);
    if (s_home.kind == 1) {
        card_set_news(s_home.category, s_home.accent, s_home.head, s_home.meta, s_home.art);
        card_set_pages(s_home.page, s_home.pages);
    } else if (s_home.kind == 2) {
        card_set_greeting(s_home.hello, s_home.head, s_home.meta, s_home.art);
    } else {
        card_set_greeting("Hello,", onboarding_get_name(), "", NULL);
    }
}

// True while the home card is what the screen shows: not in the screensaver,
// and no character moment (event, shake, wake-up) is borrowing the screen.
static bool home_visible(void)
{
    return !s_saver && !s_freeze && s_evt_stage == EVT_NONE && !s_evt_pending && s_shake_stage == SHAKE_NONE &&
           s_wake_stage == WAKE_NONE;
}

static void refresh_home_if_showing(void)
{
    if (s_home_applied) {
        apply_home();
    }
}

void character_set_home_news(const char *category, uint32_t accent, const char *headline, const char *meta,
                             const char *art, int page, int pages)
{
    s_home.kind = 1;
    snprintf(s_home.category, sizeof(s_home.category), "%s", category ? category : "");
    s_home.accent = accent;
    snprintf(s_home.head, sizeof(s_home.head), "%s", headline ? headline : "");
    snprintf(s_home.meta, sizeof(s_home.meta), "%s", meta ? meta : "");
    snprintf(s_home.art, sizeof(s_home.art), "%s", art ? art : "");
    s_home.page = page;
    s_home.pages = pages;
    refresh_home_if_showing();
}

void character_set_home_greeting(const char *hello, const char *name, const char *interests, const char *art)
{
    s_home.kind = 2;
    snprintf(s_home.hello, sizeof(s_home.hello), "%s", hello ? hello : "");
    snprintf(s_home.head, sizeof(s_home.head), "%s", name ? name : "");
    snprintf(s_home.meta, sizeof(s_home.meta), "%s", interests ? interests : "");
    snprintf(s_home.art, sizeof(s_home.art), "%s", art ? art : "");
    refresh_home_if_showing();
}

bool character_home_showing(void)
{
    return s_home_applied;
}

static bool s_cut_blinked; // the blink before the face-to-card cut has been started

bool character_ready_for_event(void)
{
    return s_active && !s_freeze && !s_focus && !s_act_running && s_emotion == CHAR_EMOTION_IDLE &&
           s_shake_stage == SHAKE_NONE && s_wake_stage == WAKE_NONE && s_evt_stage == EVT_NONE &&
           !s_evt_pending && !s_mood_active && s_card_alpha < 0.01f;
}

static void post_event(int kind, const char *header, const char *text)
{
    if (!character_ready_for_event()) {
        return;
    }
    tick_fast();
    s_evt_kind = kind;
    s_evt_news = false;
    snprintf(s_evt_header, sizeof(s_evt_header), "%s", header ? header : "");
    snprintf(s_evt_text, sizeof(s_evt_text), "%s", text ? text : "");
    s_evt_pending = true;
}

void character_show_card(char_card_kind_t kind, const char *header, const char *text)
{
    if (header && text) {
        post_event((int)kind, header, text);
    }
}

void character_show_news(const char *category, uint32_t accent, const char *headline, const char *meta,
                         const char *art)
{
    if (!category || !headline) {
        return;
    }
    post_event(CHAR_CARD_NEWS, category, headline);
    if (s_evt_pending && s_evt_kind == CHAR_CARD_NEWS) {
        s_evt_news = true;
        s_evt_accent = accent;
        snprintf(s_evt_meta, sizeof(s_evt_meta), "%s", meta ? meta : "");
        snprintf(s_evt_art, sizeof(s_evt_art), "%s", art ? art : "");
    }
}

void character_show_breathe(void)
{
    post_event(EVT_KIND_BREATHE, "", "");
}

void character_show_checkin(void)
{
    post_event(EVT_KIND_CHECKIN, "CHECK-IN", "Hey. How are you?");
}

bool character_show_low_battery(void)
{
    // Deliberately looser than character_ready_for_event(): it gets through
    // focus mode and a passing mood. It still waits for the character to be
    // awake and not mid-reaction - and never wakes a sleeping device, which
    // would only spend the little battery there is left.
    bool awake = s_emotion == CHAR_EMOTION_IDLE || s_emotion == CHAR_EMOTION_SLEEPY;
    if (!s_active || s_freeze || !awake || s_shake_stage != SHAKE_NONE || s_wake_stage != WAKE_NONE ||
        s_evt_stage != EVT_NONE || s_evt_pending || s_card_alpha >= 0.01f) {
        return false;
    }
    tick_fast();
    s_evt_kind = EVT_KIND_LOWBATT;
    s_evt_header[0] = s_evt_text[0] = '\0';
    s_evt_pending = true;
    return true;
}

// Moments that outrank everything (find my keychain, pairing code): they
// cancel whatever was happening, wake the screen, and ignore focus mode.
static void post_priority(int kind, const char *header, const char *text)
{
    uint32_t now = lv_tick_get();
    tick_fast();
    exit_deep();
    if (s_act_running) {
        acts_cancel();
        s_act_running = false;
    }
    s_wake_stage = WAKE_NONE;
    s_wake_pending = false;
    s_last_motion = now;
    leave_saver(now);
    s_evt_stage = EVT_NONE;
    card_clear_options();
    s_evt_kind = kind;
    s_evt_news = false;
    snprintf(s_evt_header, sizeof(s_evt_header), "%s", header);
    snprintf(s_evt_text, sizeof(s_evt_text), "%s", text);
    s_evt_pending = true;
}

void character_find_start(void)
{
    post_priority(EVT_KIND_FIND, "FIND ME", "Here I am!");
}

static void end_card_if(int kind)
{
    if (s_evt_kind == kind && (s_evt_stage == EVT_CARD || s_evt_stage == EVT_NOTICE || s_evt_pending)) {
        s_evt_pending = false;
        if (s_evt_stage == EVT_NONE) {
            return;
        }
        s_evt_next = lv_tick_get(); // the next tick ends the card
    }
}

void character_find_stop(void)
{
    end_card_if(EVT_KIND_FIND);
}

bool character_find_active(void)
{
    return s_evt_kind == EVT_KIND_FIND && (s_evt_pending || s_evt_stage == EVT_NOTICE || s_evt_stage == EVT_CARD);
}

void character_show_passkey(uint32_t code)
{
    char text[32];
    snprintf(text, sizeof(text), "%03u %03u\nOn your phone", (unsigned)((code / 1000) % 1000), (unsigned)(code % 1000));
    post_priority(EVT_KIND_PAIR, "PAIRING CODE", text);
}

void character_pairing_done(void)
{
    end_card_if(EVT_KIND_PAIR);
}

static void find_motion_hook(uint32_t now)
{
    // Picking the keychain up ends the search (but not in the first moments,
    // while it is still being handled by whoever asked it to ring).
    if (s_evt_kind == EVT_KIND_FIND && s_evt_stage == EVT_CARD && now - s_card_t0 > 2500) {
        s_evt_next = now;
    }
}

bool character_input_active(void)
{
    return s_evt_stage == EVT_ASK;
}

// --- Tilt-only answering of the check-in -----------------------------------

// Tilt thresholds, in gaze units (1.0 = a full look, about 0.3 g). It takes
// ENGAGE_X to wake the card up at all; then a choice is entered past ENTER_X
// and only left again once back inside LEAVE_X (the gap stops it flickering
// between two answers while the board wobbles on the boundary).
#define IN_ENGAGE_X 0.6f
#define IN_ENTER_X  0.55f
#define IN_LEAVE_X  0.30f
// How long a choice must be held to count. Level ("Okay") is where a
// device rests, so it asks for a longer, more deliberate hold.
#define IN_DWELL_SIDE_MS 1500
#define IN_DWELL_MID_MS  2400

static float s_in_x;
static bool s_in_engaged;
static int s_in_zone; // 0 Good (left), 1 Okay (level), 2 Rough (right)
static uint32_t s_in_since;

void character_input_tilt(float x)
{
    s_in_x = x;
}

static void confirm_answer(int choice, uint32_t now)
{
    s_choice = choice;
    board_haptic_feedback(BOARD_HAPTIC_NOTIFY); // answer given
    card_clear_options();
    card_set("CHECK-IN", wellness_checkin_reply(choice));
    character_set_emotion(choice == 0 ? CHAR_EMOTION_HAPPY : (choice == 1 ? CHAR_EMOTION_CALM : CHAR_EMOTION_SAD));
    character_look(0.0f, 0.0f, 300);
    s_reply_breathe = choice == 2;
    s_evt_stage = EVT_REPLY;
    s_evt_next = now + EVT_REPLY_MS;
}

// Runs every tick while a check-in is waiting for an answer.
static void ask_update(uint32_t now)
{
    float x = s_in_x;

    if (!s_in_engaged) {
        // Before the first deliberate tilt nothing can be answered, but the
        // face already listens: it stays curious, its eyes lean with the
        // device, and it brightens or droops a little as the tilt grows.
        character_set_emotion(x < -0.3f ? CHAR_EMOTION_HAPPY : (x > 0.3f ? CHAR_EMOTION_SAD : CHAR_EMOTION_CURIOUS));
        character_look(x * 0.9f, 0.0f, 200);
        if (fabsf(x) < IN_ENGAGE_X) {
            return; // still waiting for the first deliberate tilt
        }
        s_in_engaged = true;
        board_haptic_feedback(BOARD_HAPTIC_TICK);
        s_in_zone = x < 0 ? 0 : 2;
        s_in_since = now;
        s_evt_next = now + EVT_ASK_TIMEOUT_MS;
        card_set_hint("");
    } else {
        int zone = s_in_zone;
        if (zone == 1) {
            if (x < -IN_ENTER_X) {
                zone = 0;
            } else if (x > IN_ENTER_X) {
                zone = 2;
            }
        } else if (zone == 0 && x > -IN_LEAVE_X) {
            zone = 1;
        } else if (zone == 2 && x < IN_LEAVE_X) {
            zone = 1;
        }
        if (zone != s_in_zone) {
            board_haptic_feedback(BOARD_HAPTIC_TICK); // the highlight moved
            s_in_zone = zone;
            s_in_since = now;
            s_evt_next = now + EVT_ASK_TIMEOUT_MS; // still interacting: don't time out
        }
    }

    // The face answers along with the tilt: it smiles toward "Good", looks
    // downcast toward "Rough", and its eyes lean the way the device does.
    character_set_emotion(s_in_zone == 0 ? CHAR_EMOTION_HAPPY : (s_in_zone == 2 ? CHAR_EMOTION_SAD : CHAR_EMOTION_IDLE));
    character_look((s_in_zone - 1) * 0.6f, 0.0f, 200);

    uint32_t dwell = s_in_zone == 1 ? IN_DWELL_MID_MS : IN_DWELL_SIDE_MS;
    float progress = (float)(now - s_in_since) / (float)dwell;
    card_select(s_in_zone, progress);
    if (progress >= 1.0f) {
        confirm_answer(s_in_zone, now);
    }
}

void character_toggle_focus(void)
{
    character_set_focus(!s_focus);
}

void character_set_focus(bool on)
{
    if (!s_active || on == s_focus) {
        return;
    }
    uint32_t now = lv_tick_get();
    s_focus = on;
    s_focus_until = now + FOCUS_MAX_MS;
    s_led_boost = 0.35f;
    s_blink_start = now ? now : 1;
    s_mood_active = false;
    if (s_focus) {
        // Anything on its way (or on screen) is dropped.
        s_evt_stage = EVT_NONE;
        s_evt_pending = false;
        if (s_emotion != CHAR_EMOTION_SLEEPING && s_emotion != CHAR_EMOTION_SLEEPY) {
            character_set_emotion(CHAR_EMOTION_IDLE);
        }
    }
    led_set_quiet(s_focus);
}

bool character_focus_active(void)
{
    return s_focus;
}

static void event_cancel(void)
{
    s_evt_stage = EVT_NONE;
    s_evt_pending = false;
}

static void begin_return(uint32_t now)
{
    s_blink_start = now ? now : 1; // the card is cut away behind this blink
    s_evt_stage = EVT_RETURN;
    s_evt_next = now + EVT_RETURN_MS;
}

static void begin_breathe(uint32_t now, uint32_t duration_ms)
{
    s_evt_stage = EVT_BREATHE;
    s_evt_next = now + duration_ms;
    character_set_emotion(CHAR_EMOTION_BREATHE);
}

static void run_event_sequence(uint32_t now)
{
    // Shaking and waking up both outrank anything in progress.
    if (s_shake_stage != SHAKE_NONE || s_wake_stage != WAKE_NONE) {
        event_cancel();
        return;
    }

    if (s_evt_pending) {
        s_evt_pending = false;
        s_cut_blinked = false;
        if (s_evt_kind != EVT_KIND_FIND) {
            board_haptic_feedback(BOARD_HAPTIC_NOTIFY); // find has its own strong ring
        }
        s_evt_stage = EVT_NOTICE;
        s_blink_start = 0;
        s_glance_x = s_glance_y = 0.0f;
        s_glance_end = 0;
        if (s_evt_kind == EVT_KIND_LOWBATT) {
            // No notice beat and no card: the exhausted face *is* the message.
            // It is held like the breathing moment and recovers the same way.
            s_mood_active = false;
            s_led_boost = 0.6f;
            s_evt_stage = EVT_BREATHE;
            s_evt_next = now + EVT_LOWBATT_MS;
            character_set_emotion(CHAR_EMOTION_EXHAUSTED);
            return;
        }
        switch (s_evt_kind) {
        case EVT_KIND_FIND:
            s_evt_next = now + 250;
            s_led_boost = 1.0f;
            character_set_emotion(CHAR_EMOTION_SURPRISED);
            break;
        case EVT_KIND_PAIR:
            s_evt_next = now + 400;
            s_led_boost = 0.4f;
            character_set_emotion(CHAR_EMOTION_CALM);
            break;
        case CHAR_CARD_JOKE:
            s_evt_next = now + EVT_NOTICE_JOKE_MS;
            s_led_boost = 0.7f; // a playful flash
            character_set_emotion(CHAR_EMOTION_SMIRK);
            break;
        case CHAR_CARD_WELLNESS:
        case EVT_KIND_BREATHE:
            s_evt_next = now + EVT_NOTICE_CALM_MS;
            s_led_boost = 0.0f; // nothing sudden - the calm look is enough
            character_set_emotion(CHAR_EMOTION_CALM);
            break;
        default: // news, check-in
            s_evt_next = now + EVT_NOTICE_NEWS_MS;
            s_led_boost = 0.5f; // a short pulse
            character_set_emotion(CHAR_EMOTION_CURIOUS);
            break;
        }
        return;
    }

    if (s_evt_stage == EVT_ASK) {
        ask_update(now); // may move on to EVT_REPLY
    }

    if (s_evt_stage == EVT_NONE || now < s_evt_next) {
        return;
    }

    switch (s_evt_stage) {
    case EVT_NOTICE:
        // Close the eyes first; the cut to the card happens behind the blink.
        if (!s_cut_blinked && s_evt_kind != EVT_KIND_BREATHE) {
            s_cut_blinked = true;
            s_blink_start = now ? now : 1;
            s_evt_next = now + BLINK_CLOSE_MS;
            break;
        }
        s_cut_blinked = false;
        if (s_evt_kind == EVT_KIND_BREATHE) {
            begin_breathe(now, EVT_BREATHE_MS);
        } else if (s_evt_kind == EVT_KIND_CHECKIN) {
            card_clear_options();
            card_set_side(true);
            card_set(s_evt_header, s_evt_text);
            card_set_options("Good", "Okay", "Rough");
            card_set_hint("tilt to answer");
            s_in_engaged = false;
            s_in_zone = 1;
            s_in_x = 0.0f;
            s_choice = 1;
            s_evt_stage = EVT_ASK;
            s_evt_next = now + EVT_ASK_TIMEOUT_MS;
            character_set_emotion(CHAR_EMOTION_CURIOUS);
        } else {
            card_clear_options();
            if (s_evt_news) {
                card_set_news(s_evt_header, s_evt_accent, s_evt_text, s_evt_meta, s_evt_art);
            } else {
                card_set_side(true);
                card_set(s_evt_header, s_evt_text);
                if (s_evt_kind == CHAR_CARD_WELLNESS) {
                    card_set_footer("rep 1 of 3");
                }
            }
            s_evt_stage = EVT_CARD;
            s_card_t0 = now;
            s_card_hold = s_evt_kind == CHAR_CARD_WELLNESS ? (uint32_t)EX_REP_MS * EX_REPS + 600
                          : (s_evt_kind == EVT_KIND_FIND ? 60000u
                             : (s_evt_kind == EVT_KIND_PAIR ? 45000u : card_hold_ms(s_evt_kind, strlen(s_evt_text))));
            s_evt_next = now + s_card_hold;
            character_set_emotion(s_evt_kind == CHAR_CARD_JOKE ? CHAR_EMOTION_SMIRK
                                  : (s_evt_kind == CHAR_CARD_WELLNESS || s_evt_kind == EVT_KIND_PAIR
                                         ? CHAR_EMOTION_CALM
                                         : (s_evt_kind == EVT_KIND_FIND ? CHAR_EMOTION_HAPPY : CHAR_EMOTION_IDLE)));
        }
        break;
    case EVT_BREATHE:
        // Open the eyes with a deliberate blink.
        character_set_emotion(CHAR_EMOTION_IDLE);
        s_blink_start = now ? now : 1;
        begin_return(now);
        break;
    case EVT_REPLY:
        if (s_reply_breathe) {
            s_reply_breathe = false;
            begin_breathe(now, EVT_BREATHE_MS - 2000);
        } else {
            // A good answer leaves a brief smile; an "okay" a calm look. The
            // mood timer hands control back to neutral on its own.
            character_set_emotion(s_choice == 0 ? CHAR_EMOTION_HAPPY : CHAR_EMOTION_CALM);
            s_mood_active = true;
            s_mood_end = now + 2400;
            begin_return(now);
        }
        break;
    case EVT_CARD:
    case EVT_ASK: // unanswered: fade away quietly, never nag
        card_clear_options();
        // The face stays in whatever it was doing (a joke's smirk, the
        // exercise strain, a check-in lean) until here: settle it.
        character_set_emotion(CHAR_EMOTION_IDLE);
        s_blink_start = now ? now : 1;
        begin_return(now);
        break;
    default:
        s_evt_stage = EVT_NONE;
        s_next_glance = now + rand_between(GLANCE_MIN_MS, GLANCE_MAX_MS);
        s_next_blink = now + rand_between(1500, 3500);
        s_next_mood = now + rand_between(MOOD_MIN_MS, MOOD_MAX_MS);
        break;
    }
}

static void face_set_visibility(float alpha)
{
    if (alpha < 0.01f) {
        lv_obj_add_flag(s_face_layer, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s_face_layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(s_face_layer, alpha >= 0.99f ? LV_OPA_COVER : (lv_opa_t)(alpha * 255.0f), 0);
}

void character_leave(void)
{
    s_active = false;
    exit_deep();
    s_tick_timer = NULL; // the timer is deleted by the caller
    led_off();
    // Other faces expect the normal brightness whatever stage this one was in.
    s_bl_cur = CHAR_BACKLIGHT_AWAKE_PCT;
    s_bl_shown = CHAR_BACKLIGHT_AWAKE_PCT;
    board_set_backlight(CHAR_BACKLIGHT_AWAKE_PCT);
}

#ifdef CHARACTER_DEBUG_ENV
// CHAR_WAKE_AT_MS=<ms since start>: fakes one burst of movement at that time.
static uint32_t s_debug_wake_at;
static bool s_debug_wake_done;
// CHAR_ANSWER=<0|1|2>: answers a check-in by itself (0 Good, 1 Okay, 2 Rough)
// by feeding character_input_tilt() the way a person's tilting would: level
// for a second, then tilted toward the answer (for "Okay": tilted right to
// wake the card, then back to level), and held.
static int s_debug_answer = -1;
static uint32_t s_debug_ask_seen;
// CHAR_ACT=<1..4> starts that left-alone act (1 huff, 2 doodle, 3 rocket,
// 4 paper) two seconds after start-up, without waiting out the neglect timer.
static int s_debug_act;
static bool s_debug_act_done;
static uint32_t s_debug_act_at;

static uint32_t s_debug_find_at, s_debug_pair_at;

static void debug_answer_tick(uint32_t now)
{
    if (s_debug_find_at && now >= s_debug_find_at) {
        s_debug_find_at = 0;
        character_find_start();
    }
    if (s_debug_pair_at && now >= s_debug_pair_at) {
        s_debug_pair_at = 0;
        character_show_passkey(482913);
    }
    if (s_debug_answer < 0 || s_evt_stage != EVT_ASK) {
        s_debug_ask_seen = 0;
        return;
    }
    if (s_debug_ask_seen == 0) {
        s_debug_ask_seen = now;
    }
    uint32_t t = now - s_debug_ask_seen;
    float x = 0.0f;
    if (t >= 1000) {
        x = s_debug_answer == 0 ? -1.0f : 1.0f;
    }
    if (s_debug_answer == 1 && t >= 1800) {
        x = 0.0f;
    }
    character_input_tilt(x);
}
#endif

// Decides what is on screen this tick: the home card, a character moment's
// card, or the face. Cuts only, never fades (see card_up()). When the face
// comes back from being hidden it starts from a neutral look with its eyes
// opening, so the cut does not show a stale expression.
static bool s_layers_init;
static bool s_card_on, s_face_on;

static void update_layers(uint32_t now)
{
    bool hv = home_visible();
    bool cu = card_up(now);
    s_card_alpha = cu ? 1.0f : 0.0f; // "an event card is up" for the schedulers

    if (hv && !s_home_applied) {
        s_home_applied = true;
        apply_home();
        led_off(); // the home card is quiet: no glow
    } else if (!hv) {
        s_home_applied = false;
    }

    bool card_on = hv || cu;
    bool face_on = !hv && !(cu && !evt_kind_is_side());
    if (!s_layers_init || card_on != s_card_on) {
        s_card_on = card_on;
        card_set_visibility(card_on ? 1.0f : 0.0f);
    }
    if (!s_layers_init || face_on != s_face_on) {
        if (face_on && s_layers_init) {
            memcpy(&s_cur, expression_params(CHAR_EMOTION_IDLE), sizeof(s_cur));
            s_look_x = s_look_y = 0.0f;
            s_blink_start = now > BLINK_CLOSE_MS ? now - BLINK_CLOSE_MS : 1; // eyes open from shut
        }
        s_face_on = face_on;
        face_set_visibility(face_on ? 1.0f : 0.0f);
    }
    s_layers_init = true;
}

// What the shrunken face does while its card is up: blink now and then, and
//   joke      read the bubble, then turn to look at you with the smirk
//   wellness  do the exercise - lift a dumbbell, strain at the top, ease down
// (the check-in's face follows the tilt; see ask_update). Everything here costs
// nothing outside a side card.
static void run_side_behaviour(uint32_t now)
{
    // The exercise runs only while the card is up in its own stage; once the
    // card has ended the face must be left to settle (not held in a strain).
    bool wellness = s_evt_stage == EVT_CARD && s_evt_kind == CHAR_CARD_WELLNESS;
    bool side = side_card_showing(now);

    // Layout target and its glide.
    char_layout_t tgt = {1.0f, 0.0f, 0.0f};
    if (side) {
        tgt.scale = SIDE_SCALE;
        tgt.ox = SIDE_OX;
        tgt.oy = wellness ? SIDE_OY + 10.0f : SIDE_OY;
    }
    // Snaps (no glide): it changes while the eyes are shut, and resizing the
    // eyes frame by frame is the costly part.
    s_layout = tgt;

    if (!side) {
        acts_exercise(false, 0.0f, NULL);
        return;
    }

    if (s_blink_start == 0 && now >= s_next_blink && s_cur.eye_h > 0.6f) {
        s_blink_start = now ? now : 1;
        s_next_blink = now + rand_between(2500, 5000);
    }

    uint32_t t = now - s_card_t0;
    if (s_evt_kind == CHAR_CARD_JOKE && s_evt_stage == EVT_CARD) {
        if (t < s_card_hold * 55 / 100) {
            character_look(0.9f, -0.25f, 120); // reading it
        } else {
            character_look(0.0f, 0.1f, 120);   // ...and a knowing look at you
        }
    } else if (s_evt_kind == EVT_KIND_FIND && s_evt_stage == EVT_CARD) {
        // "Here I am!": the face looks left and right and flips between wide-eyed
        // and smiling, with a flash of the LED on every beat (1.2 s).
        uint32_t phase = t % 1200;
        if (phase < 40) {
            s_led_boost = 1.0f;
        }
        character_set_emotion(phase < 600 ? CHAR_EMOTION_SURPRISED : CHAR_EMOTION_HAPPY);
        character_look(phase < 600 ? -0.7f : 0.7f, -0.15f, 150);
    } else if (wellness) {
        float p = (float)(t % EX_REP_MS) / (float)EX_REP_MS;
        float lift = 0.5f - 0.5f * cosf(p * 2.0f * 3.14159265f);
        character_set_emotion(p < 0.5f && lift > 0.3f ? CHAR_EMOTION_EFFORT : CHAR_EMOTION_CALM);
        char_layout_t eff = s_layout;
        eff.oy -= lift * 6.0f; // up on the toes
        acts_exercise(true, lift, &eff);
        int rep = (int)(t / EX_REP_MS) + 1;
        if (rep > EX_REPS) {
            rep = EX_REPS;
        }
        static int s_shown_rep;
        if (rep != s_shown_rep) {
            s_shown_rep = rep;
            char buf[16];
            snprintf(buf, sizeof(buf), "rep %d of %d", rep, EX_REPS);
            card_set_footer(buf);
        }
    }
}

static void engine_tick(lv_timer_t *timer)
{
    s_tick_timer = timer; // so deep sleep can slow this timer down and back up
    uint32_t now = lv_tick_get();

#ifdef CHARACTER_DEBUG_ENV
    if (s_debug_wake_at && !s_debug_wake_done && now >= s_debug_wake_at) {
        s_debug_wake_done = true;
        character_motion(1.0f);
    }
#endif

    // Deep sleep: fully asleep for DEEP_AFTER_MS with nothing going on. While
    // deep there is nothing to draw or animate (the screen is off), so the
    // tick does no work at all; waking is done by character_motion().
    bool asleep = s_emotion == CHAR_EMOTION_SLEEPING && !s_freeze && s_shake_stage == SHAKE_NONE &&
                  s_wake_stage == WAKE_NONE && s_evt_stage == EVT_NONE;
    if (asleep) {
        if (s_sleep_since == 0) {
            s_sleep_since = now ? now : 1;
        }
    } else {
        s_sleep_since = 0;
    }
    if (s_deep) {
        if (asleep) {
            return;
        }
        exit_deep(); // something other than motion changed the state
    } else if (asleep && now - s_sleep_since >= s_deep_after) {
        enter_deep();
        return;
    }

    run_shake_sequence(now);
    run_wake_sequence(now);
    run_event_sequence(now);
    run_side_behaviour(now);
#ifdef CHARACTER_DEBUG_ENV
    debug_answer_tick(now);
    if (s_debug_act && !s_debug_act_done && now >= s_debug_act_at && !s_act_running &&
        s_emotion == CHAR_EMOTION_IDLE) {
        s_debug_act_done = true;
        s_act_running = acts_start((act_id_t)s_debug_act, now);
    }
#endif

    // After SAVER_AFTER_MS without any movement the home card gives way to the
    // character (the screensaver).
    if (!s_saver && !s_freeze && s_evt_stage == EVT_NONE && !s_evt_pending && s_shake_stage == SHAKE_NONE &&
        s_wake_stage == WAKE_NONE && now - s_last_motion >= s_saver_after) {
        enter_saver(now);
    }

    // Left-alone acts: only on the idle screensaver, never over a card or
    // another sequence, and dropped the instant anything moves the device
    // (character_motion). Each companion has its own (acts/acts.h).
    if (s_act_running) {
        act_out_t o;
        if (acts_tick(now, &o)) {
            character_set_emotion(o.emotion);
            if (o.has_gaze) {
                s_override_x = o.gaze_x;
                s_override_y = o.gaze_y;
                s_override_until = now + 120;
            }
        } else {
            s_act_running = false;
            character_set_emotion(o.emotion);
            if (o.emotion != CHAR_EMOTION_IDLE) {
                s_mood_active = true; // settles back to neutral on its own
                s_mood_end = now + 1500;
            }
            s_next_act = now + (s_demo_saver ? 2500u : rand_between(NEGLECT_GAP_MIN_MS, NEGLECT_GAP_MAX_MS));
            s_next_glance = now + rand_between(GLANCE_MIN_MS, GLANCE_MAX_MS);
            s_next_blink = now + rand_between(1500, 3500);
        }
    } else if (s_saver && !s_freeze && !s_focus && s_emotion == CHAR_EMOTION_IDLE && s_shake_stage == SHAKE_NONE &&
               s_wake_stage == WAKE_NONE && s_evt_stage == EVT_NONE && !s_mood_active &&
               s_card_alpha < 0.01f && now >= s_next_act) {
        if (acts_start(acts_pick(onboarding_get_companion()), now)) {
            s_act_running = true;
            s_blink_start = 0;
            s_glance_x = s_glance_y = 0.0f;
            s_glance_end = 0;
        }
    }

#if CHARACTER_SHOWCASE
    if (!s_freeze && s_shake_stage == SHAKE_NONE && now >= s_showcase_next) {
        s_showcase_index = (s_showcase_index + 1) % CHAR_EMOTION_COUNT;
        s_mood_active = false;
        character_set_emotion((char_emotion_t)s_showcase_index);
        s_showcase_next = now + SHOWCASE_STEP_MS;
    }
#endif

    // The shake, wake and card sequences own the emotion until they finish.
    if (s_saver && !s_freeze && s_shake_stage == SHAKE_NONE && s_wake_stage == WAKE_NONE &&
        s_evt_stage == EVT_NONE && !s_act_running) {
        run_sleep_schedule(now);
        run_idle_behaviour(now);
    }

    update_layers(now);
    char_layout_t lay = s_layout;
    if (s_face_on) { // nothing of the face is computed or drawn while the home card is up
    const char_params_t *target = expression_params(s_emotion);
    float k = expression_speed(s_emotion);
    tween_params(&s_cur, target, k);

    // Gaze = caller override, else idle glance, plus the emotion's own bias.
    float gx, gy;
    if (now < s_override_until) {
        gx = s_override_x;
        gy = s_override_y;
    } else {
        gx = s_glance_x + s_cur.look_x;
        gy = s_glance_y + s_cur.look_y;
    }
    float look_k = 0.28f;
    // Resting breath is a ~4 s cycle; the deliberate breathing moment is a
    // slower 5 s cycle (and the LED pulses on the same period).
    float breath_period = s_emotion == CHAR_EMOTION_BREATHE ? (float)BREATH_PERIOD_MS : 4000.0f;
    float dy = sinf((float)now * (2.0f * 3.14159265f / breath_period)) * s_cur.breath;

    if (s_emotion == CHAR_EMOTION_DIZZY) {
        // Pupils orbit (one lap every 0.75 s), the face bobs and the mouth
        // wobbles between a smile and a frown - all faster than anything
        // else the character does, so it reads as "not quite right".
        float a = (float)now * (2.0f * 3.14159265f / 750.0f);
        gx = cosf(a) * 0.95f;
        gy = sinf(a) * 0.95f;
        look_k = 0.7f;
        dy = sinf(a * 0.5f) * 3.0f;
        s_cur.mouth_curve = sinf(a * 0.5f) * 0.55f;
    }

    gx = fmaxf(-1.0f, fminf(1.0f, gx));
    gy = fmaxf(-1.0f, fminf(1.0f, gy));
    tween(&s_look_x, gx, look_k);
    tween(&s_look_y, gy, look_k);

    if (s_evt_stage == EVT_CARD && s_evt_kind == CHAR_CARD_WELLNESS) {
        float p = (float)((now - s_card_t0) % EX_REP_MS) / (float)EX_REP_MS;
        lay.oy -= (0.5f - 0.5f * cosf(p * 2.0f * 3.14159265f)) * 6.0f;
    }
    eyes_apply(&s_cur, s_look_x, s_look_y, blink_amount(now), dy, &lay);
    mouth_apply(&s_cur, dy, &lay);

    s_led_boost *= 0.94f;
    led_apply(s_emotion, now, s_led_boost);
    }
    update_backlight();

    // Focus: switches itself off after 90 minutes, and its "focused." line
    // fades in and out (hidden while drowsy or asleep - it would just clutter).
    if (s_focus && now >= s_focus_until) {
        s_focus = false;
        led_set_quiet(false);
    }
    float focus_target = (s_focus && s_emotion != CHAR_EMOTION_SLEEPING && s_emotion != CHAR_EMOTION_SLEEPY &&
                          s_evt_stage == EVT_NONE) ? 1.0f : 0.0f;
    if (s_focus_alpha != focus_target) {
        s_focus_alpha += (focus_target - s_focus_alpha) * 0.12f;
        if (fabsf(focus_target - s_focus_alpha) < 0.01f) {
            s_focus_alpha = focus_target;
        }
        extras_set_focus(s_focus_alpha);
    }

    status_apply(now);
    if (s_face_on) {
        extras_apply(&s_cur, now, &lay);
    }

    // With a static card up there is nothing to animate, so the engine ticks
    // slowly (post_event / shake / movement call tick_fast() to speed it up).
    uint32_t want = (s_home_applied && !s_deep) ? HOME_TICK_MS : TICK_MS;
    if (want != s_tick_ms && !s_deep) {
        s_tick_ms = want;
        lv_timer_set_period(timer, want);
    }
}

lv_timer_t *character_build(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_center(root);
    lv_obj_set_style_bg_color(root, lv_color_hex(s_theme->bg), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    // The face parts live in their own layer so the whole face can dissolve
    // out while a card is up; the status strip sits on the root, above both.
    s_face_layer = lv_obj_create(root);
    lv_obj_remove_style_all(s_face_layer);
    lv_obj_clear_flag(s_face_layer, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s_face_layer, lv_pct(100), lv_pct(100));
    lv_obj_center(s_face_layer);

    mouth_build(s_face_layer, s_theme);
    eyes_build(s_face_layer, s_theme, onboarding_get_companion() == COMPANION_FEMALE);
    extras_build(s_face_layer, s_theme);
    acts_build(s_face_layer, s_theme);
    card_build(root, s_theme);
    status_build(root, s_theme);

    s_act_running = false;
    s_next_act = lv_tick_get() + s_neglect_first;
    s_active = true;
    s_deep = false;
    s_sleep_since = 0;
    s_bl_cur = CHAR_BACKLIGHT_AWAKE_PCT;
    s_bl_shown = CHAR_BACKLIGHT_AWAKE_PCT;
    s_focus_alpha = 0.0f; // the new label starts hidden; the tick fades it in if focus is on
    s_evt_stage = EVT_NONE;
    s_evt_pending = false;
    s_card_alpha = 0.0f;
    s_saver = false;
    s_saver_since = 0;
    s_exit_saver_after_wake = false;
    s_home_applied = false;
    s_layers_init = false;
    s_tick_ms = TICK_MS;
    s_layout = (char_layout_t){1.0f, 0.0f, 0.0f};
    s_shake_stage = SHAKE_NONE;
    s_shake_pending = false;
    s_wake_stage = WAKE_NONE;
    s_wake_pending = false;
    s_led_boost = 0.0f;
    s_last_motion = lv_tick_get();

    // Starts at idle, already settled (no tween-in from zeros on a face
    // switch), with every scheduler timed from "now".
    uint32_t now = lv_tick_get();
    s_emotion = CHAR_EMOTION_IDLE;
    memcpy(&s_cur, expression_params(s_emotion), sizeof(s_cur));
    s_look_x = s_look_y = 0.0f;
    s_glance_x = s_glance_y = 0.0f;
    s_override_until = 0;
    s_blink_start = 0;
    s_mood_active = false;
    s_next_blink = now + rand_between(1200, 3000);
    s_next_glance = now + rand_between(GLANCE_MIN_MS, GLANCE_MAX_MS);
    s_glance_end = 0;
    s_next_mood = now + rand_between(MOOD_MIN_MS, MOOD_MAX_MS);
#if CHARACTER_SHOWCASE
    s_showcase_index = 0;
    s_showcase_next = now + SHOWCASE_STEP_MS;
#endif

#ifdef CHARACTER_DEBUG_ENV
    const char *pin = getenv("CHAR_EMOTION");
    if (pin) {
        int v = atoi(pin);
        if (v >= 0 && v < CHAR_EMOTION_COUNT) {
            s_emotion = (char_emotion_t)v;
            memcpy(&s_cur, expression_params(s_emotion), sizeof(s_cur));
            s_freeze = true;
        }
    }
    // CHAR_SHAKE=1 plays the shake reaction on start-up, bypassing the IMU.
    if (getenv("CHAR_SHAKE")) {
        character_on_shake();
    }
    // CHAR_STILL_MS=<ms> shortens the stillness-to-sleep delays so the
    // sleep and wake animations can be seen without waiting minutes.
    const char *still = getenv("CHAR_STILL_MS");
    if (still && atoi(still) > 0) {
        s_sleepy_after = (uint32_t)atoi(still);
        s_sleep_after = s_sleepy_after * 8 / 5;
    }
    const char *answer = getenv("CHAR_ANSWER");
    if (answer) {
        s_debug_answer = atoi(answer);
    }
    // CHAR_FOCUS=1 switches focus mode on at start-up (a long press of BOOT).
    if (getenv("CHAR_FOCUS")) {
        character_toggle_focus();
    }
    const char *act = getenv("CHAR_ACT");
    if (act && atoi(act) > 0) {
        s_debug_act = atoi(act);
        s_debug_act_done = false;
        s_debug_act_at = lv_tick_get() + 2000;
    }
    // CHAR_NEGLECT_MS=<ms>: how long before the first left-alone act.
    const char *neglect = getenv("CHAR_NEGLECT_MS");
    if (neglect && atoi(neglect) > 0) {
        s_neglect_first = (uint32_t)atoi(neglect);
        s_next_act = lv_tick_get() + s_neglect_first;
    }
    // CHAR_DEEP_MS=<ms>: how long fully asleep before going deep.
    const char *deep_ms = getenv("CHAR_DEEP_MS");
    if (deep_ms && atoi(deep_ms) > 0) {
        s_deep_after = (uint32_t)atoi(deep_ms);
    }
    const char *find_at = getenv("CHAR_FIND_AT_MS"); // find-my-keychain card, after this many ms
    if (find_at && atoi(find_at) > 0) {
        s_debug_find_at = lv_tick_get() + (uint32_t)atoi(find_at);
    }
    const char *pair_at = getenv("CHAR_PAIR_AT_MS"); // the pairing-code card
    if (pair_at && atoi(pair_at) > 0) {
        s_debug_pair_at = lv_tick_get() + (uint32_t)atoi(pair_at);
    }
    const char *wake_at = getenv("CHAR_WAKE_AT_MS");
    if (wake_at && atoi(wake_at) > 0) {
        s_debug_wake_at = lv_tick_get() + (uint32_t)atoi(wake_at);
        s_debug_wake_done = false;
    }
#endif

#ifdef CHARACTER_DEBUG_ENV
    // CHAR_SAVER_MS=<ms>: how long without movement before the screensaver.
    // Any of the face-testing variables above starts straight in the screensaver
    // (they are about the character), as does CHAR_SAVER=1.
    const char *saver_ms = getenv("CHAR_SAVER_MS");
    if (saver_ms && atoi(saver_ms) > 0) {
        s_saver_after = (uint32_t)atoi(saver_ms);
    }
    if (getenv("CHAR_SAVER") || getenv("CHAR_EMOTION") || getenv("CHAR_ACT") || getenv("CHAR_STILL_MS") ||
        getenv("CHAR_WAKE_AT_MS") || getenv("CHAR_NEGLECT_MS") || getenv("CHAR_DEEP_MS")) {
        s_saver = true;
        s_saver_since = now;
    }
#endif

    lv_timer_t *timer = lv_timer_create(engine_tick, TICK_MS, NULL);
    engine_tick(timer);
    return timer;
}

void character_set_emotion(char_emotion_t emotion)
{
    if (emotion < CHAR_EMOTION_COUNT) {
        s_emotion = emotion;
    }
}

void character_look(float x, float y, uint32_t hold_ms)
{
    s_override_x = x;
    s_override_y = y;
    s_override_until = lv_tick_get() + hold_ms;
}
