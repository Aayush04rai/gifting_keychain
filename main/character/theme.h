#pragma once

#include <stdint.h>

// Colours for every part, in one place so a "personality pack" (Executive,
// Tech, Wellness, Retro) is a different instance of this struct and nothing
// else. Only the Executive pack exists so far.
typedef struct {
    uint32_t bg;
    uint32_t eye;
    uint32_t pupil;
    uint32_t glint;
    uint32_t mouth;
    uint32_t extra;
} char_theme_t;

static const char_theme_t CHAR_THEME_EXECUTIVE = {
    .bg = 0x000000,
    .eye = 0xe6edf3,   // cool off-white
    .pupil = 0x0d1b2a, // deep navy rather than pure black
    .glint = 0xffffff,
    .mouth = 0xd4af6a, // the product's gold accent, used sparingly
    .extra = 0x8795a3, // muted slate: z / dots stay secondary but legible
};
