#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "character.h"
#include "params.h"
#include "onboarding.h"
#include "theme.h"

// Acts: the short things the companion does when it has been left alone for a
// while and nobody has noticed it. They happen on the character screensaver,
// never over the professional UI, and the moment anyone touches the device the
// act is dropped and the character reacts to being noticed.
//
// Each companion has two, in the same register as the rest of the character:
// playful, small, and never at anyone's expense.
//
//   female:  HUFF    - an exaggerated "hmph": slanted brows, a frown, a couple of
//                      puffs of steam, glancing at you to check if you noticed.
//            DOODLE  - quietly draws something on its screen (a heart, a star or
//                      a spiral), admires it, and smiles.
//   male:    ROCKET  - assembles a little rocket between its eyes, counts down,
//                      and launches it off the top of the screen.
//            PAPER   - screws up a ball of paper and flicks it at a bin across
//                      the screen; it goes in, or doesn't (and it meant that).
//
// The colours are the usual ones for both: only what is drawn differs.
typedef enum {
    ACT_NONE = 0,
    ACT_HUFF,
    ACT_DOODLE,
    ACT_ROCKET,
    ACT_PAPER,
} act_id_t;

typedef struct {
    char_emotion_t emotion; // what the face should be doing right now
    bool has_gaze;          // if set, where the eyes should be looking
    float gaze_x, gaze_y;
} act_out_t;

// Creates the props (hidden) on the face layer. Call once per character build.
void acts_build(lv_obj_t *face_layer, const char_theme_t *theme);

// One of this companion's acts, chosen at random but never the same one twice
// in a row.
act_id_t acts_pick(companion_t who);

// Starts an act. Returns false if one is already running.
bool acts_start(act_id_t id, uint32_t now_ms);

// Advances the running act. Returns true while it is running, filling *out
// with what the face should do. Returns false when it has just finished (or
// none was running), with *out holding the face to settle into.
bool acts_tick(uint32_t now_ms, act_out_t *out);

// The wellness exercise: a dumbbell held overhead, with two arms, that rises and
// falls with the face (lift 0 = down, 1 = fully raised). It is not an "act" -
// the card sequence in character.c drives it directly, on the shrunken face -
// so it only needs placing relative to the current face layout. Call with
// on = false to hide it.
void acts_exercise(bool on, float lift, const char_layout_t *lay);

// Stops the current act at once and hides its props.
void acts_cancel(void);
