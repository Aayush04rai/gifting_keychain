#include "expressions.h"

// Fields in order: eye_w eye_h lid_top lid_bot asym dome pupil
//                  mouth_curve mouth_alpha mouth_open extra_q extra_z
//                  extra_dots breath look_x look_y right_open
//
// Most of the personality is restraint: IDLE is almost a blank stare, and
// every other expression changes only the two or three parameters that carry
// it (curious = one lid up + a "?", happy = arched lower lids, ...).
static const char_params_t k_params[CHAR_EMOTION_COUNT] = {
    [CHAR_EMOTION_IDLE]      = {1.00f, 1.00f, 0.00f, 0.00f, 0.00f, 0.0f, 1.00f,
                                0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
    [CHAR_EMOTION_CURIOUS]   = {1.00f, 1.06f, 0.10f, 0.00f, 0.70f, 0.0f, 1.12f,
                                0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.45f, -0.25f, 1.0f, 0.0f, 0.0f, 0.0f},
    [CHAR_EMOTION_HAPPY]     = {1.00f, 0.92f, 0.00f, 0.42f, 0.00f, 1.0f, 1.00f,
                                0.9f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
    [CHAR_EMOTION_SURPRISED] = {1.08f, 1.28f, 0.00f, 0.00f, 0.00f, 0.0f, 0.55f,
                                0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
    [CHAR_EMOTION_THINKING]  = {1.00f, 1.00f, 0.20f, 0.00f, -0.45f, 0.0f, 1.00f,
                                0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.75f, -0.85f, 1.0f, 0.0f, 0.0f, 0.0f},
    [CHAR_EMOTION_SLEEPY]    = {1.00f, 0.95f, 0.52f, 0.06f, 0.00f, 0.0f, 0.90f,
                                0.0f, 0.0f, 0.0f, 0.0f, 0.35f, 0.0f, 2.0f, 0.0f, 0.55f, 1.0f, 0.0f, 0.0f, 0.0f},
    [CHAR_EMOTION_SLEEPING]  = {1.00f, 0.10f, 0.00f, 0.00f, 0.00f, 0.0f, 0.00f,
                                0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 3.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
    // Dizzy: the pupils circle and the mouth wobbles - both driven by the
    // engine (character.c), so the table only sets the base look.
    [CHAR_EMOTION_DIZZY]     = {1.00f, 1.04f, 0.00f, 0.00f, 0.00f, 0.0f, 0.78f,
                                0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
    // Waking, step 1: only the left eye cracks open, heavy-lidded; the right
    // stays a thin line.
    [CHAR_EMOTION_WAKING1]   = {1.00f, 0.70f, 0.38f, 0.04f, 0.00f, 0.0f, 0.85f,
                                0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.5f, 0.0f, 0.35f, 0.10f, 0.0f, 0.0f, 0.0f},
    // Waking, step 2: both eyes open, still a little sleepy.
    [CHAR_EMOTION_WAKING2]   = {1.00f, 0.98f, 0.30f, 0.02f, 0.00f, 0.0f, 0.95f,
                                0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.5f, 0.0f, 0.2f, 1.0f, 0.0f, 0.0f, 0.0f},
    // Smirk: one lid lower than the other, a small knowing smile, eyes cut
    // sideways - the "I have something for you" look before a joke.
    [CHAR_EMOTION_SMIRK]     = {1.00f, 0.92f, 0.30f, 0.06f, -0.75f, 0.0f, 0.95f,
                                0.8f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.55f, 0.05f, 1.0f, 0.0f, 0.0f, 0.0f},
    // Calm: soft, slightly lowered lids with a gentle arch and a faint smile -
    // content rather than amused. Used before a wellness nudge.
    [CHAR_EMOTION_CALM]      = {1.00f, 0.90f, 0.24f, 0.12f, 0.00f, 0.6f, 1.00f,
                                0.4f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.15f, 1.0f, 0.0f, 0.0f, 0.0f},
    // Breathe: eyes closed to thin lines, a trace of a smile, and a large
    // slow rise and fall (character.c runs this one on a 5 s breath).
    [CHAR_EMOTION_BREATHE]   = {1.00f, 0.10f, 0.00f, 0.00f, 0.00f, 0.0f, 0.00f,
                                0.3f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 6.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
    // Exhausted (low battery): heavy drooping lids, a downturned mouth, eyes
    // sagging toward the floor, a heaving breath, the whole face washed
    // toward red, and a sweat drop. Cute rather than alarming: the red is a
    // soft blush of it, not a siren.
    [CHAR_EMOTION_EXHAUSTED] = {1.00f, 0.78f, 0.36f, 0.12f, 0.00f, 0.0f, 0.75f,
                                -0.7f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 3.5f, 0.0f, 0.6f, 1.0f, 1.0f, 1.0f, 0.0f},
    // Angry: lids slanted so the inner ends sit low (the classic brow), a
    // tight downturned mouth, pupils a touch small. Reads as "hmph", never
    // as menace: the eyes stay round and the colours stay the usual ones.
    [CHAR_EMOTION_ANGRY]     = {1.00f, 0.92f, 0.34f, 0.04f, 0.00f, 0.0f, 0.80f,
                                -0.85f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 24.0f},
    // Effort: eyes squeezed behind slanted lids, a small puffing "o", a bead of
    // sweat - the wellness exercise's lifting face.
    [CHAR_EMOTION_EFFORT]    = {1.04f, 0.84f, 0.22f, 0.10f, 0.00f, 0.0f, 0.85f,
                                0.0f, 0.0f, 0.55f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 12.0f},
    // Sad: lids slanted the other way (outer ends low), a frown, eyes down.
    [CHAR_EMOTION_SAD]       = {1.00f, 0.96f, 0.30f, 0.04f, 0.00f, 0.0f, 0.90f,
                                -0.65f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.5f, 0.0f, 0.35f, 1.0f, 0.0f, 0.0f, -14.0f},
};

static const float k_speed[CHAR_EMOTION_COUNT] = {
    [CHAR_EMOTION_IDLE]      = 0.18f,
    [CHAR_EMOTION_CURIOUS]   = 0.22f,
    [CHAR_EMOTION_HAPPY]     = 0.20f,
    [CHAR_EMOTION_SURPRISED] = 0.50f,
    [CHAR_EMOTION_THINKING]  = 0.16f,
    [CHAR_EMOTION_SLEEPY]    = 0.07f,
    [CHAR_EMOTION_SLEEPING]  = 0.06f,
    [CHAR_EMOTION_DIZZY]     = 0.30f,
    [CHAR_EMOTION_WAKING1]   = 0.14f,
    [CHAR_EMOTION_WAKING2]   = 0.16f,
    [CHAR_EMOTION_SMIRK]     = 0.20f,
    [CHAR_EMOTION_CALM]      = 0.10f,
    [CHAR_EMOTION_BREATHE]   = 0.07f,
    [CHAR_EMOTION_EXHAUSTED] = 0.12f,
    [CHAR_EMOTION_ANGRY]     = 0.28f,
    [CHAR_EMOTION_EFFORT]    = 0.30f,
    [CHAR_EMOTION_SAD]       = 0.14f,
};

const char_params_t *expression_params(char_emotion_t emotion)
{
    return &k_params[emotion];
}

float expression_speed(char_emotion_t emotion)
{
    return k_speed[emotion];
}
