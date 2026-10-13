#pragma once

// Everything a part needs to draw one frame. All fields are continuous, so
// the engine can tween between any two expressions field by field.
// Where and how big the face is drawn. The full face fills the screen
// (scale 1, no offset); cards that talk *to* you shrink it into the left of
// the screen (scale ~0.58) and put their text on the right. The engine tweens
// this like any other value, so the face glides over rather than jumping.
typedef struct {
    float scale;  // 1 = full size
    float ox, oy; // offset of the face's centre from the layer's centre, px
} char_layout_t;

typedef struct {
    float eye_w;       // eye width scale, 1 = base
    float eye_h;       // eye height scale; ~0.1 reads as a closed eye
    float lid_top;     // fraction of eye height covered from the top, 0..1
    float lid_bot;     // fraction covered from the bottom, 0..1
    float asym;        // -1..1: >0 lowers the left lid / raises the right
    float dome;        // 0..1: rounds the lower lid into a "happy" arch
    float pupil;       // pupil size scale
    float mouth_curve; // -1 frown .. +1 smile
    float mouth_alpha; // 0 hides the smile/frown arc
    float mouth_open;  // 0..1 "o" mouth, for surprise
    float extra_q;     // opacity of the "?" extra
    float extra_z;     // opacity of the "z" extra
    float extra_dots;  // opacity of the thinking "..." extra
    float breath;      // vertical breathing amplitude in px
    float look_x;      // default gaze bias for this emotion, -1..1
    float look_y;
    float right_open;  // 0..1: how open the right eye is; <1 for the one-eye-first wake-up
    float tint;        // 0..1: blend the eyes, pupils and mouth toward alarm red
    float extra_sweat; // opacity of the sweat drop beside the face
    float lid_tilt;    // degrees: tilts both upper lids so their inner ends sit low (angry brows)
} char_params_t;
