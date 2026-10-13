#pragma once

#include "character.h"
#include "params.h"

// Target parameters for an emotion.
const char_params_t *expression_params(char_emotion_t emotion);

// How fast the live params chase the target, per 33 ms tick (0..1).
// Surprise snaps; sleepiness drifts.
float expression_speed(char_emotion_t emotion);
