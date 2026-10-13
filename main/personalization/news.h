#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t interest;   // the single ONBOARD_INTEREST_* bit this headline is for
    char category[20];   // short upper-case label for the card header, e.g. "CYBERSECURITY"
    char title[104];     // plain-ASCII headline, ready for the display font
    char source[28];     // publisher, e.g. "Reuters" (plain ASCII; may be empty)
    int64_t published;   // UTC seconds since 1970, or 0 if the feed did not say
} news_item_t;

// Starts background headline fetching for the topics the recipient picked
// during onboarding. Does nothing if they picked none.
//
// Real hardware (news_esp32.c): once WiFi is up, asks GNews for one topic
// roughly every 18 minutes, rotating through the recipient's topics, so the
// free tier's 100 requests/day is never exceeded however many topics are
// selected. Stays off (with one log line) while GNEWS_API_KEY in
// config/api_keys.h is empty.
// Simulator (news_sim.c): no network; serves canned headlines from
// config/sim_config.h.
void news_start(void);

// Copies the oldest headline that has not been shown yet into *out, marks it
// shown, and returns true; returns false if there is nothing new. Each
// headline is handed out at most once.
bool news_take(news_item_t *out);

// True if news is configured to work at all (topics selected and, on real
// hardware, an API key present). Lets the scheduler skip it cheaply.
bool news_available(void);
