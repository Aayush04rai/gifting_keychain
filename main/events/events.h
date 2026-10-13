#pragma once

// The Event Engine, first version: decides *when* the companion has something
// to say, and hands it to the character as a card (see character.h).
//
// What it posts today:
//   - a news headline, when a fresh one is waiting for a topic the recipient
//     picked (news.h), spaced well apart and capped per day;
//   - a short joke (jokes.h), roughly one to three a day at unpredictable
//     times, so each still lands as a surprise.
//
// Rules it keeps, so the device stays a companion and not a notification
// machine:
//   - nothing between 20:00 and 09:00 local time (and nothing at all until
//     the clock has been set);
//   - nothing while the character is asleep, dizzy, mid-reaction or not the
//     face on screen - it simply waits for the next quiet moment;
//   - a minimum gap between any two events.
//
// It also posts the wellness layer, all of it kept deliberately light:
//   - a wellness moment two or three times a day, far apart: half the time a
//     wordless breathing pause, half a short kind line ("Shoulders down.");
//   - a once-a-day check-in ("How are you?" - Good / Okay / Rough), offered
//     between 10:00 and 17:00 and never repeated if ignored.
// And focus mode (lay the device face-down for about two seconds; face-up
// again ends it) silences every one of these, jokes and news included, until
// it is switched off.
//
// Demo mode: when non-zero, the device plays a fixed show-reel of every screen
// (greeting, headlines, joke, wellness exercise, breathing, check-in, low
// battery, screensaver with its acts - see post_demo in events.c), starting each
// scene this-many seconds after the previous one ends, ignoring the quiet
// hours, gaps and daily caps - for filming and demos. MUST be 0 for the real product, where the
// rules above are the whole point.
#ifndef EVENTS_DEMO_SECONDS
#define EVENTS_DEMO_SECONDS 9
#endif

// Call once after news_start()/jokes_start(). Needs the LVGL task running.
void events_start(void);
