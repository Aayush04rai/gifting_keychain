#pragma once

#include <stddef.h>
#include <stdint.h>

#include "onboarding.h"

// One row per interest that has a news source. `query` is the tail of a GNews
// URL (endpoint plus its topic parameters, already URL-encoded); the caller
// appends language, count and the API key.
//   search?q=...            free-text search across headlines
//   top-headlines?category= GNews' own curated category feed
typedef struct {
    uint16_t bit;
    const char *label;
    const char *query;
    uint32_t accent;   // the category's tint on the card (chip, glow)
    const char *art;   // LVGL path of its artwork on the SD card (see tools/make_art.py)
} news_topic_t;

static const news_topic_t NEWS_TOPICS[] = {
    {ONBOARD_INTEREST_AI, "AI", "search?q=%22artificial%20intelligence%22", 0x5b8cff, "S:ui/cat_ai.bin"},
    {ONBOARD_INTEREST_ROBOTICS, "ROBOTICS", "search?q=robotics", 0x5b8cff, "S:ui/cat_robotics.bin"},
    {ONBOARD_INTEREST_SEMI, "SEMICONDUCTORS", "search?q=semiconductor", 0x6aa8ff, "S:ui/cat_semi.bin"},
    {ONBOARD_INTEREST_SPACE, "SPACE", "search?q=space%20exploration", 0x7a7dff, "S:ui/cat_space.bin"},
    {ONBOARD_INTEREST_TECH, "TECHNOLOGY", "top-headlines?category=technology", 0x3ec6d8, "S:ui/cat_tech.bin"},
    {ONBOARD_INTEREST_CYBER, "CYBERSECURITY", "search?q=cybersecurity", 0xb06bff, "S:ui/cat_cyber.bin"},
    {ONBOARD_INTEREST_FINANCE, "MARKETS", "top-headlines?category=business", 0x2fd08f, "S:ui/cat_finance.bin"},
    {ONBOARD_INTEREST_EV, "ELECTRIC VEHICLES", "search?q=%22electric%20vehicles%22", 0x39d98a, "S:ui/cat_ev.bin"},
    {ONBOARD_INTEREST_HEALTH, "HEALTHCARE", "top-headlines?category=health", 0x3fd0b8, "S:ui/cat_health.bin"},
    {ONBOARD_INTEREST_DEFENCE, "DEFENCE", "search?q=defence%20OR%20defense", 0xe0a050, "S:ui/cat_defence.bin"},
    {ONBOARD_INTEREST_TRAVEL, "TRAVEL", "search?q=travel%20industry", 0x5cc8ff, "S:ui/cat_travel.bin"},
    {ONBOARD_INTEREST_F1, "FORMULA 1", "search?q=%22formula%201%22", 0xff6b5b, "S:ui/cat_f1.bin"},
    {ONBOARD_INTEREST_FOOTBALL, "FOOTBALL", "search?q=football", 0x7bd35b, "S:ui/cat_football.bin"},
};

#define NEWS_TOPIC_COUNT (sizeof(NEWS_TOPICS) / sizeof(NEWS_TOPICS[0]))
