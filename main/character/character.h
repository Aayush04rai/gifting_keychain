#pragma once

#include "lvgl.h"

// Character Engine - one recognizable companion whose face is *composed* from
// parts (eyes/, mouth/, extras/) driven by a handful of parameters, rather
// than a set of pre-drawn frames. An emotion is just a target parameter set
// (see expressions.c); the engine tweens the live parameters toward it every
// tick, so any two emotions blend smoothly and new ones are a table row.
//
//   emotion + attention + context  ->  target params  ->  tween  ->  parts
//
// Layout: main/character/
//   character.c      engine: tween, blink, idle behaviour, public API
//   expressions.c    emotion -> target params table
//   eyes/ mouth/ extras/   one folder per part type, each a self-contained
//                          "build once, apply(params) every tick" module
//   theme.h          colours; the hook for future personality packs

typedef enum {
    CHAR_EMOTION_IDLE = 0,
    CHAR_EMOTION_CURIOUS,
    CHAR_EMOTION_HAPPY,
    CHAR_EMOTION_SURPRISED,
    CHAR_EMOTION_THINKING,
    CHAR_EMOTION_SLEEPY,
    CHAR_EMOTION_SLEEPING,
    CHAR_EMOTION_DIZZY,
    CHAR_EMOTION_WAKING1,
    CHAR_EMOTION_WAKING2,
    CHAR_EMOTION_SMIRK,
    CHAR_EMOTION_CALM,
    CHAR_EMOTION_BREATHE,
    CHAR_EMOTION_EXHAUSTED,
    CHAR_EMOTION_ANGRY,
    CHAR_EMOTION_EFFORT, // straining: squeezed eyes, puffing mouth, a bead of sweat (exercise)
    CHAR_EMOTION_SAD,    // outer lids low, a frown (a rough day)
    CHAR_EMOTION_COUNT,
} char_emotion_t;

// Backlight brightness (%) while the character is awake. The drowsy stages
// dim from here, and main.c sets it at start-up.
#define CHAR_BACKLIGHT_AWAKE_PCT 80

// True while the device is in deep sleep (screen off, chip light-sleeping).
// Other periodic tasks use it to slow themselves down, so the chip has real
// quiet to sleep in.
bool character_is_deep(void);

// Builds the character into `parent` (which should be an unscrolled box the
// size of the area the face may use) and returns the engine's update timer,
// matching the watchface build-function contract used by ui.c.
lv_timer_t *character_build(lv_obj_t *parent);

// Smoothly transitions to `emotion`. Idempotent for the current emotion.
void character_set_emotion(char_emotion_t emotion);

// Where the character is looking, each axis -1..1 (x: left..right,
// y: up..down). Overrides the random idle glances for `hold_ms`; the IMU
// reactions (phase 3) will call this every poll.
void character_look(float x, float y, uint32_t hold_ms);

// The device was shaken or jerked. Plays surprised -> dizzy -> recover ->
// idle (about 3.5 s), then hands control back to the normal behaviour.
// Safe to call whether or not the character is currently on screen.
void character_on_shake(void);

// --- Information cards ---------------------------------------------------
//
// The character "notices something", the face gives way to a small card with
// a category line and some text, then the face comes back:
//
//   idle -> notice (curious for news, a smirk for a joke) -> card -> idle
//
typedef enum {
    CHAR_CARD_NEWS,
    CHAR_CARD_JOKE,
    CHAR_CARD_WELLNESS, // a calm look first, then a short kind line
} char_card_kind_t;

// True when the character is on screen, awake and doing nothing in
// particular, i.e. a good moment to interrupt it. Checked by the scheduler
// before it posts anything; events are never forced on a sleeping, dizzy or
// otherwise busy character.
bool character_ready_for_event(void);

// Starts the notice -> card -> return sequence. `header` is a short label
// (e.g. "CYBERSECURITY"), `text` the headline or joke (plain ASCII). Both are
// copied. Ignored if the character is not ready (see above). How long the
// card stays up scales with the text length.
void character_show_card(char_card_kind_t kind, const char *header, const char *text);

// A news card with the full layout (category chip, headline, a meta line such as
// "Reuters - 2h ago", and artwork from the SD card if present): same notice -> card
// -> return sequence as character_show_card(). ccent is the category tint
// (0xRRGGBB); rt is an LVGL image path or NULL. Ignored if not ready.
void character_show_news(const char *category, uint32_t accent, const char *headline, const char *meta,
                         const char *art);

// --- The home card -------------------------------------------------------
//
// The device's front face: an information card the character is not part of.
// The character appears only as a screensaver (after about five minutes
// without movement) and for the short moments above (jokes, wellness, the
// check-in, low battery). events.c feeds this; the content is held here so it
// can be redrawn whenever the home card returns to the screen.
void character_set_home_news(const char *category, uint32_t accent, const char *headline, const char *meta,
                             const char *art, int page, int pages);
void character_set_home_greeting(const char *hello, const char *name, const char *interests, const char *art);
bool character_home_showing(void); // the home card is on screen right now

// Demo only: switches to the screensaver now (instead of after minutes of
// stillness) and starts the first left-alone act after `act_delay_ms`, with
// the following ones a few seconds apart. Ended by character_motion(). Returns
// false if the character is busy.
bool character_demo_saver(uint32_t act_delay_ms);

// The wordless wellness moment: the character looks calm, closes its eyes,
// breathes slowly for about ten seconds (the LED breathing with it), then
// opens its eyes and carries on. No text, no card. Ignored if not ready.
void character_show_breathe(void);

// Low battery: the face turns red and exhausted - drooping lids, a sagging
// mouth, a sweat drop, the LED pulsing red - for about six seconds, then
// recovers. No text; the face is the message. Unlike every other moment it is
// allowed through focus mode (a flat battery is not an interruption you want
// to miss), but not while the character is asleep, shaking or mid-reaction;
// returns false in those cases so the caller can try again shortly.
bool character_show_low_battery(void);

// Find my keychain. The phone asks the keychain to announce itself (ble_find.c):
// it wakes whatever it was doing, the screen shows "Here I am!" with the face
// looking about, and the LED pulses (ble_find.c adds the haptic buzz). It goes
// on until character_find_stop(), or until the keychain is picked up, or 60 s.
void character_find_start(void);
void character_find_stop(void);
bool character_find_active(void);

// Bluetooth pairing: shows the 6-digit code the phone must be given, in the same
// side-bubble style, until character_pairing_done() or 45 s.
void character_show_passkey(uint32_t code);
void character_pairing_done(void);

// The daily check-in: a card asking "How are you?" with Good / Okay / Rough.
// It needs no button and no touch - only tilt:
//
//   - Nothing is selected at first (a keychain sitting on a desk must never
//     answer for itself); the card just says "tilt to answer".
//   - The first deliberate tilt engages it. After that the highlight follows
//     the tilt: left = Good, level = Okay, right = Rough.
//   - Holding a choice fills its pill; when the fill completes (1.5 s for
//     Good/Rough, 2.4 s for Okay, which is the resting position) that is the
//     answer. Tilting elsewhere before then resets the fill.
//   - Not engaged, or engaged but not finished, for 20 s: the card fades away
//     quietly and the character never asks again that day.
//
// The character replies with one supportive line (after "Rough" it also takes
// a breath with them). Nothing is stored. Ignored if not ready.
void character_show_checkin(void);

// True while a check-in is waiting for an answer. While it is, gesture.c
// forwards the tilt here (and does nothing else with it) every poll.
bool character_input_active(void);

// Current sideways tilt measured from the resting position, in the same
// units as the gaze: -1 is a full tilt left (left edge down), +1 a full tilt
// right. Feed it every poll while character_input_active().
void character_input_tilt(float x);

// Focus / do-not-disturb. While on: no jokes, news, wellness prompts or
// check-ins, fewer idle glances, no unprompted moods, a dimmer LED and a quiet
// "focused." line under the eyes. The character still blinks and still reacts
// to being moved. Switches itself off after 90 minutes so it can't be
// forgotten.
//
// The device has no buttons, so the way in is a gesture (gesture.c): lay it
// face-down and leave it there for about two seconds - like silencing a phone
// - and turn it face-up again to end it. (A long press of BOOT, on the
// development board, still toggles it.) Only acts while the companion is the
// face on screen.
void character_set_focus(bool on);
void character_toggle_focus(void);
bool character_focus_active(void);

// Called when the character stops being the visible face, so the LED does
// not keep glowing on faces that have nothing to do with it.
void character_leave(void);

// Feed the engine how much the device is moving: the size of the change in
// acceleration (in g) since the previous poll, or 1.0 for a button press.
// Sustained near-zero values put the character to sleep (sleepy, then
// asleep); any real movement wakes it. Call every IMU poll.
void character_motion(float delta_g);

// Showcase mode steps through every emotion on a timer so each can be
// reviewed without the event engine (which arrives in phase 2). Off is the
// product behaviour: the character mostly just idles.
#ifndef CHARACTER_SHOWCASE
#define CHARACTER_SHOWCASE 0
#endif
