#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lvgl.h"

#include "board_hal.h"
#include "character.h"
#include "events.h"
#include "jokes.h"
#include "news.h"
#include "news_topics.h"
#include "onboarding.h"
#include "wellness.h"

// All gaps are in milliseconds of uptime.
#define MIN_GAP_ANY_MS   (20u * 60 * 1000)      // between any two events
#define JOKE_PER_DAY     3
// Jokes arrive at unpredictable times, a few hours apart; the first one a
// little after the device is first switched on.
#define JOKE_FIRST_MIN_MS (40u * 60 * 1000)
#define JOKE_FIRST_MAX_MS (90u * 60 * 1000)
#define JOKE_GAP_MIN_MS   (3u * 3600 * 1000)
#define JOKE_GAP_MAX_MS   (8u * 3600 * 1000)
// Wellness nudges are rarer and further apart than anything else: a couple a
// day, never close together. Half are the wordless breathing moment.
#define WELLNESS_PER_DAY      3
#define WELLNESS_FIRST_MIN_MS (60u * 60 * 1000)
#define WELLNESS_FIRST_MAX_MS (120u * 60 * 1000)
#define WELLNESS_GAP_MIN_MS   (150u * 60 * 1000)
#define WELLNESS_GAP_MAX_MS   (300u * 60 * 1000)

#define DAY_START_HOUR 9
#define DAY_END_HOUR   20
// The check-in asks once a day, mid-morning onward, and gives up by 17:00 - a
// "how are you" at the end of the day would feel like a different question.
#define CHECKIN_START_HOUR 10
#define CHECKIN_END_HOUR   17

#define TICK_MS 1000
#define TICK_DEEP_MS 5000

static uint32_t s_last_any;
static uint32_t s_next_joke;
static uint32_t s_next_wellness;
static bool s_have_last_any;

static int s_day_key;       // year * 366 + yday, to notice midnight
static int s_joke_today;
static int s_wellness_today;
static int s_checkin_day = -1; // the day_key a check-in was last offered on

// Demo mode (see EVENTS_DEMO_SECONDS in events.h): post an event on a short
// fixed beat instead of following the real schedule, cycling through every
// kind. The simulator can also switch it on with EVENT_FAST=<seconds>
// (sim/CMakeLists.txt defines EVENTS_DEBUG_ENV for that).
static uint32_t s_fast_ms;
static uint32_t s_fast_next;
static int s_fast_idx;

static uint32_t rand_between(uint32_t lo, uint32_t hi)
{
    return lo + lv_rand(0, hi - lo);
}

// True while it is a sensible hour to say something. False until the clock
// has actually been set (it reads 1970 before the first time sync).
static bool in_active_hours(struct tm *out_tm)
{
    // Plain localtime() (not _r/_s): the one call that exists unchanged on
    // both the firmware and the MSVC simulator. Only ever called from the
    // display task, so its shared buffer is not a hazard.
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (!tm_info || tm_info->tm_year + 1900 < 2024) {
        return false;
    }
    if (out_tm) {
        *out_tm = *tm_info;
    }
    return tm_info->tm_hour >= DAY_START_HOUR && tm_info->tm_hour < DAY_END_HOUR;
}

static void roll_day(const struct tm *tm_info)
{
    int key = (tm_info->tm_year + 1900) * 366 + tm_info->tm_yday;
    if (key != s_day_key) {
        s_day_key = key;
        s_joke_today = 0;
        s_wellness_today = 0;
    }
}

static void mark_any(uint32_t now)
{
    s_last_any = now;
    s_have_last_any = true;
}

static void post_joke(uint32_t now)
{
    character_show_card(CHAR_CARD_JOKE, "ONE-LINER", joke_next());
    mark_any(now);
    s_joke_today++;
    s_next_joke = now + rand_between(JOKE_GAP_MIN_MS, JOKE_GAP_MAX_MS);
}

// --- The home card ----------------------------------------------------------
//
// The front face of the device is an information card for the recipient's
// interests (character.h: character_set_home_*). Every HOME_REFRESH_MS it moves
// on to the next of the last few headlines (fresh ones from news.h are
// collected as they arrive); until the first headline exists it shows a
// greeting. The character is only the screensaver and an occasional visitor.
#define HOME_REFRESH_MS (4u * 60 * 1000)
#define HOME_KEEP       6
#define HOME_GREETING_ART "S:ui/home.bin"

static news_item_t s_items[HOME_KEEP];
static int s_item_count;
static int s_item_w;
static int s_pick;
static uint32_t s_home_next;
static uint32_t s_home_shown_at;
static bool s_home_greeting;

static void relative_age(int64_t published, char *out, size_t n)
{
    out[0] = 0;
    time_t now = time(NULL);
    if (published > 0 && now > 1700000000 && now >= published) {
        long mins = (long)((now - published) / 60);
        if (mins < 60) {
            snprintf(out, n, "%ldm ago", mins < 1 ? 1 : mins);
        } else if (mins < 48 * 60) {
            snprintf(out, n, "%ldh ago", mins / 60);
        } else {
            snprintf(out, n, "%ldd ago", mins / 1440);
        }
    }
}

static void home_show_item(const news_item_t *item, int page, int pages)
{
    uint32_t accent = 0x5b8cff;
    const char *art = NULL;
    for (size_t i = 0; i < NEWS_TOPIC_COUNT; i++) {
        if (NEWS_TOPICS[i].bit == item->interest) {
            accent = NEWS_TOPICS[i].accent;
            art = NEWS_TOPICS[i].art;
            break;
        }
    }
    char age[16];
    relative_age(item->published, age, sizeof(age));
    char meta[48];
    if (item->source[0] && age[0]) {
        snprintf(meta, sizeof(meta), "%.27s - %s", item->source, age);
    } else {
        snprintf(meta, sizeof(meta), "%.27s%s", item->source, age);
    }
    character_set_home_news(item->category, accent, item->title, meta, art, page, pages);
    s_home_greeting = false;
}

// "AI  -  Robotics  -  Semiconductors", from the topics the recipient picked.
static void interests_line(char *out, size_t n)
{
    out[0] = 0;
    uint16_t mask = onboarding_get_interests();
    int shown = 0;
    for (size_t i = 0; i < NEWS_TOPIC_COUNT && shown < 3; i++) {
        if (!(mask & NEWS_TOPICS[i].bit)) {
            continue;
        }
        char word[24];
        snprintf(word, sizeof(word), "%s", NEWS_TOPICS[i].label);
        if (strlen(word) > 3) { // "ROBOTICS" -> "Robotics"; short ones ("AI") stay as they are
            for (char *c = word + 1; *c; c++) {
                if (*c >= 'A' && *c <= 'Z') {
                    *c = (char)(*c - 'A' + 'a');
                }
            }
        }
        size_t used = strlen(out);
        snprintf(out + used, n - used, "%s%s", shown ? "  -  " : "", word);
        shown++;
    }
}

static void home_show_greeting(void)
{
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    const char *hello = "Hello,";
    if (tm_info && tm_info->tm_year + 1900 >= 2024) {
        hello = tm_info->tm_hour < 12 ? "Good morning," : (tm_info->tm_hour < 17 ? "Good afternoon," : "Good evening,");
    }
    char interests[48];
    interests_line(interests, sizeof(interests));
    character_set_home_greeting(hello, onboarding_get_name(), interests, HOME_GREETING_ART);
    s_home_greeting = true;
    s_home_shown_at = lv_tick_get();
}

// Built-in headlines for demo mode, so the home card can be seen without an
// API key. Only used when EVENTS_DEMO_SECONDS is set.
static void home_demo_fill(void)
{
    static const struct {
        uint16_t bit;
        const char *label, *title, *source;
        int hours;
    } samples[] = {
        {ONBOARD_INTEREST_AI, "AI", "Open-source model matches frontier systems on reasoning benchmarks", "Daily Wire Desk", 2},
        {ONBOARD_INTEREST_SPACE, "SPACE", "Next mission to study distant galaxies cleared for launch", "Space Weekly", 3},
        {ONBOARD_INTEREST_CYBER, "CYBERSECURITY", "New vulnerability found in popular open-source library", "Cyber Brief", 5},
        {ONBOARD_INTEREST_FINANCE, "MARKETS", "Markets close higher as tech stocks rally", "Market Desk", 1},
    };
    for (int k = 0; k < 4; k++) {
        news_item_t *it = &s_items[s_item_w % HOME_KEEP];
        memset(it, 0, sizeof(*it));
        it->interest = samples[k].bit;
        snprintf(it->category, sizeof(it->category), "%s", samples[k].label);
        snprintf(it->title, sizeof(it->title), "%s", samples[k].title);
        snprintf(it->source, sizeof(it->source), "%s", samples[k].source);
        it->published = (int64_t)time(NULL) - samples[k].hours * 3600;
        s_item_w++;
        if (s_item_count < HOME_KEEP) {
            s_item_count++;
        }
    }
}

static void home_refresh(void)
{
    static bool s_first = true;
    if (s_first && EVENTS_DEMO_SECONDS > 0) {
        s_first = false;
        home_show_greeting(); // demo mode opens on the greeting, then moves to headlines
        return;
    }
    s_first = false;
    news_item_t fresh;
    for (int taken = 0; taken < HOME_KEEP && news_available() && news_take(&fresh); taken++) {
        s_items[s_item_w % HOME_KEEP] = fresh;
        s_item_w++;
        if (s_item_count < HOME_KEEP) {
            s_item_count++;
        }
    }
    if (s_item_count == 0 && EVENTS_DEMO_SECONDS > 0) {
        home_demo_fill();
    }
    if (s_item_count == 0) {
        home_show_greeting();
        return;
    }
    // The ring holds the newest HOME_KEEP; walk it oldest-to-newest, forever.
    int idx = (s_pick++) % s_item_count;
    int base = s_item_count < HOME_KEEP ? 0 : s_item_w % HOME_KEEP;
    home_show_item(&s_items[(base + idx) % HOME_KEEP], idx, s_item_count);
    s_home_shown_at = lv_tick_get();
}

static void post_wellness(uint32_t now)
{
    // Half the time say nothing at all: the breathing moment is the point.
    if (lv_rand(0, 1) == 0) {
        character_show_breathe();
    } else {
        character_show_card(CHAR_CARD_WELLNESS, "A MOMENT", wellness_next());
    }
    mark_any(now);
    s_wellness_today++;
    s_next_wellness = now + rand_between(WELLNESS_GAP_MIN_MS, WELLNESS_GAP_MAX_MS);
}

// Demo mode: a fixed show-reel of every screen, for filming and demos. Scenes
// run in order and loop; the next one starts EVENTS_DEMO_SECONDS after the
// previous one has finished (so the home card always gets to be read):
//
//    0 home: next headline            6 home: next headline
//    1 joke (character, side bubble)  7 check-in (tilt to answer)
//    2 home: next headline            8 home: next headline
//    3 wellness exercise              9 low battery
//    4 home: next headline           10 screensaver: the character, with its
//    5 breathing moment                  left-alone acts, for ~30 s, then
//                                        the device is "picked up"
#define DEMO_SCENES 11
#define DEMO_SAVER_MS 30000u
static uint32_t s_demo_wake_at; // when the demo screensaver ends (0 = not in it)
static bool s_demo_busy;        // a character moment is running; the next beat waits for it

// Returns false if the scene could not start yet (try again next tick).
static bool post_demo(uint32_t now)
{
    switch (s_fast_idx % DEMO_SCENES) {
    case 1:
        character_show_card(CHAR_CARD_JOKE, "ONE-LINER", joke_next());
        s_demo_busy = true;
        break;
    case 3:
        character_show_card(CHAR_CARD_WELLNESS, "A MOMENT", wellness_next());
        s_demo_busy = true;
        break;
    case 5:
        character_show_breathe();
        s_demo_busy = true;
        break;
    case 7:
        character_show_checkin();
        s_demo_busy = true;
        break;
    case 9:
        // The exhausted low-battery face, shown on demand so it can be
        // previewed without draining a real battery.
        if (!character_show_low_battery()) {
            return false;
        }
        s_demo_busy = true;
        break;
    case 10:
        if (!character_demo_saver(2500)) {
            return false;
        }
        s_demo_wake_at = now + DEMO_SAVER_MS;
        break;
    default: // 0, 2, 4, 6, 8
        home_refresh(); // the home card moves on; no character moment
        break;
    }
    s_fast_idx++;
    return true;
}

// --- Low battery ----------------------------------------------------------
//
// The one thing that gets through focus mode. Escalates, but never nags:
// once at 15%, once at 8%, then every 20 minutes at 5% or less. It re-arms
// only after the battery has climbed back over 25% (i.e. been charged).
#define BATT_CHECK_MS     30000u
#define BATT_ALERT_1_PCT  15
#define BATT_ALERT_2_PCT  8
#define BATT_ALERT_3_PCT  5
#define BATT_REARM_PCT    25
#define BATT_REPEAT_MS    (20u * 60 * 1000)

static uint32_t s_last_batt_check;
static bool s_batt_checked;
static int s_batt_stage;          // 0 none yet, 1 at 15%, 2 at 8%, 3 at 5%
static uint32_t s_last_batt_alert;

static void check_battery(uint32_t now)
{
    if (s_batt_checked && now - s_last_batt_check < BATT_CHECK_MS) {
        return;
    }
    s_batt_checked = true;
    s_last_batt_check = now;

    int pct = board_get_battery_pct();
    if (pct < 0 || board_battery_charging()) {
        return; // no battery fitted / not measurable, or it is being charged
    }
    if (pct >= BATT_REARM_PCT) {
        s_batt_stage = 0;
        return;
    }

    int level = pct <= BATT_ALERT_3_PCT ? 3 : (pct <= BATT_ALERT_2_PCT ? 2 : (pct <= BATT_ALERT_1_PCT ? 1 : 0));
    bool due = level > s_batt_stage || (level == 3 && now - s_last_batt_alert >= BATT_REPEAT_MS);
    if (due && character_show_low_battery()) {
        s_batt_stage = level;
        s_last_batt_alert = now;
    } else if (due) {
        s_batt_checked = false; // the character was busy: look again next tick
    }
}

static void events_tick(lv_timer_t *timer)
{
    // Nothing here can fire while the device is deeply asleep (the character
    // is not ready), so tick far less often then - every tick is a wake-up
    // for the chip.
    static bool s_slow;
    bool deep = character_is_deep();
    if (deep != s_slow) {
        s_slow = deep;
        lv_timer_set_period(timer, deep ? TICK_DEEP_MS : TICK_MS);
    }
    uint32_t now = lv_tick_get();

    check_battery(now);

    // The home card refreshes on its own clock, whatever the character is doing.
    if (now >= s_home_next) {
        s_home_next = now + HOME_REFRESH_MS;
        home_refresh();
    } else if (!s_fast_ms && s_home_greeting && now - s_home_shown_at > 15000 && news_available()) {
        s_home_next = now + HOME_REFRESH_MS;
        home_refresh(); // the first headline has arrived: swap the greeting for it
    }

    if (s_fast_ms) {
        // Demo show-reel (see post_demo). The demo screensaver ends by itself.
        if (s_demo_wake_at) {
            if (now >= s_demo_wake_at) {
                s_demo_wake_at = 0;
                character_motion(1.0f); // "picked up": back to the home card
                s_fast_next = now + s_fast_ms;
            }
            return;
        }
        bool ready = character_ready_for_event();
        if (s_demo_busy && ready) {
            s_demo_busy = false; // the moment is over: start the gap before the next scene
            s_fast_next = now + s_fast_ms;
        }
        if (ready && !s_demo_busy && now >= s_fast_next) {
            if (post_demo(now)) {
                s_fast_next = now + s_fast_ms; // instant scenes (home) just linger this long
            }
        }
        return;
    }

    // Nothing is ever forced on a busy, sleeping, focused or off-screen
    // character (focus mode makes this false for as long as it is on).
    if (!character_ready_for_event()) {
        return;
    }

    struct tm tm_info;
    if (!in_active_hours(&tm_info)) {
        return;
    }
    roll_day(&tm_info);

    if (s_have_last_any && now - s_last_any < MIN_GAP_ANY_MS) {
        return;
    }

    // The daily check-in goes first: it happens once, in a window, and would
    // otherwise be crowded out by everything else.
    bool in_checkin_window = tm_info.tm_hour >= CHECKIN_START_HOUR && tm_info.tm_hour < CHECKIN_END_HOUR;
    if (in_checkin_window && s_checkin_day != s_day_key) {
        s_checkin_day = s_day_key; // offered once; whether answered or not
        character_show_checkin();
        mark_any(now);
        return;
    }

    if (s_wellness_today < WELLNESS_PER_DAY && now >= s_next_wellness) {
        post_wellness(now);
        return;
    }

    if (s_joke_today < JOKE_PER_DAY && now >= s_next_joke) {
        post_joke(now);
    }
}

void events_start(void)
{
    uint32_t now = lv_tick_get();
    s_next_joke = now + rand_between(JOKE_FIRST_MIN_MS, JOKE_FIRST_MAX_MS);
    s_next_wellness = now + rand_between(WELLNESS_FIRST_MIN_MS, WELLNESS_FIRST_MAX_MS);

    if (EVENTS_DEMO_SECONDS > 0) {
        s_fast_ms = (uint32_t)EVENTS_DEMO_SECONDS * 1000u;
        s_fast_next = now + 9000u; // the greeting is shown first
    }
#ifdef EVENTS_DEBUG_ENV
    const char *fast = getenv("EVENT_FAST");
    if (fast && atoi(fast) > 0) {
        s_fast_ms = (uint32_t)atoi(fast) * 1000u;
        s_fast_next = now + s_fast_ms;
    }
    // EVENT_FIRST=<0..4> starts the demo cycle at a given kind: 0 news,
    // 1 joke, 2 wellness text, 3 breathe, 4 check-in.
    const char *first = getenv("EVENT_FIRST");
    if (first) {
        s_fast_idx = atoi(first);
    }
#endif

    // This may be called from the main task while the display task is
    // already running, so create the timer under the display lock.
    if (board_lock(1000)) {
        lv_timer_create(events_tick, TICK_MS, NULL);
        board_unlock();
    }
}
