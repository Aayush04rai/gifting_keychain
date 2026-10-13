#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    bool valid;         // false until the first successful fetch (or in the simulator, always true once started)
    char team1[24];
    char team2[24];
    char score1[20];    // e.g. "187-4"; empty if the API didn't have an innings score yet
    char overs1[12];    // e.g. "20.0 ov"
    char score2[20];
    char overs2[12];
    char status[80];    // free-text match summary, e.g. "Australia need 46 runs from 22 balls"
} cricket_score_t;

// Starts background polling for a live cricket score, gated on the
// recipient having checked "cricket" during onboarding (onboarding.h's
// ONBOARD_INTEREST_CRICKET) - does nothing otherwise, so no network/CPU
// cost for recipients who aren't interested in cricket.
//
// Real hardware (cricket_esp32.c): polls CricketData.org's free-tier
// currentMatches endpoint over HTTPS every 15 minutes once WiFi is
// connected (see wifi_sta.h and config/api_keys.h for the API key).
// Simulator (cricket_sim.c): no network calls - returns a canned score
// from config/sim_config.h so the cricket face has something to render.
void cricket_start(void);

// Copies the latest fetched score into *out and returns true, or returns
// false (leaving *out untouched) if nothing has been fetched yet (still
// waiting on the first poll, WiFi isn't connected, or the interest wasn't
// selected during onboarding).
bool cricket_get(cricket_score_t *out);
