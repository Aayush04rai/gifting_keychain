#pragma once

// Tunables for the desktop simulator only (sim/main_sim.c, sim/board_sim.c).
// Nothing here affects the real firmware - see board_config.h for hardware
// facts shared by both, and lv_conf_sim.h for the simulator's LVGL build.

#define SIM_WINDOW_TITLE "ESP32-S3-LCD-1.47B simulator"

// Scales the small panel up for visibility on a desktop monitor; the
// underlying LVGL canvas stays at BOARD_LCD_H_RES x BOARD_LCD_V_RES
// (board_config.h) so the UI is pixel-accurate to the real device.
#define SIM_WINDOW_ZOOM 3.0f

// Fake WiFi scan: how long a scan "takes" and the networks it "finds",
// since there's no real radio to scan with on a desktop.
#define SIM_WIFI_SCAN_DELAY_MS 1200

#define SIM_WIFI_FAKE_AP_LIST \
    {"CorporateGifting-5G", -42}, \
    {"CorporateGifting-2G", -55}, \
    {"Guest-WiFi", -61}, \
    {"IoT-Lab", -70},

// Fake battery and Wi-Fi for the status bar. Override at run time with the
// SIM_BATTERY (0-100, or -1 for "no battery") and SIM_RSSI (dBm, or 0 for
// "not connected") environment variables to see the low/offline states.
#define SIM_FAKE_BATTERY_PCT 76
#define SIM_FAKE_WIFI_RSSI   (-58)

// The simulator has no real heap constraint to measure, so this is just a
// plausible fixed number for anything that wants to display it.
#define SIM_FAKE_FREE_HEAP_KB 210

// No captive portal to simulate on a desktop; onboarding_sim.c reports
// these placeholders immediately instead (see main/onboarding/onboarding.h).
#define SIM_DEV_NAME "Alex"
#define SIM_DEV_TITLE "Chief Technology Officer"
// Bitmask matching main/onboarding/onboarding.h's ONBOARD_INTEREST_* bits
// (0=none, bit0=cricket, bit1=f1, bit2=football, bit3=tech, bit4=finance,
// bit5=travel). 0x09 = tech + cricket, so the cricket face (gated on that
// bit) shows up in the gallery for testing.
// 0x2059 = tech + cricket + finance + AI (bit 6) + cybersecurity (bit 13),
// so the news reactions have several categories to draw from.
#define SIM_DEV_INTERESTS 0x2059

// Which companion the simulator shows by default (COMPANION_MALE or
// COMPANION_FEMALE; the SIM_COMPANION environment variable overrides it).
#define SIM_DEV_COMPANION COMPANION_FEMALE

// Canned headlines for the simulator's news source (no network calls):
// {interest bit, headline}. main/personalization/news_sim.c rotates through
// the ones whose bit the recipient selected.
#define SIM_NEWS_LIST \
    {ONBOARD_INTEREST_AI, "Open-source model matches frontier systems on reasoning benchmarks"}, \
    {ONBOARD_INTEREST_AI, "Chipmaker unveils next-generation AI accelerator platform"}, \
    {ONBOARD_INTEREST_CYBER, "Researchers disclose critical flaw in widely used VPN appliances"}, \
    {ONBOARD_INTEREST_FINANCE, "Global markets steady as investors weigh central bank signals"}, \
    {ONBOARD_INTEREST_TECH, "Cloud providers expand regional capacity to meet enterprise demand"}

// Canned live score for the cricket face in the simulator - no real
// network calls happen here (see main/personalization/cricket_sim.c);
// main/personalization/cricket_esp32.c does the actual CricketData.org
// fetch on real hardware.
#define SIM_CRICKET_TEAM1  "India"
#define SIM_CRICKET_TEAM2  "Australia"
#define SIM_CRICKET_SCORE1 "187-4"
#define SIM_CRICKET_OVERS1 "20.0 ov"
#define SIM_CRICKET_SCORE2 "142-6"
#define SIM_CRICKET_OVERS2 "16.2 ov"
#define SIM_CRICKET_STATUS "Australia need 46 runs from 22 balls"

/*We''l now add widgets too
Like weather info pulled from freely available apis to show current weather(Sunny, cloudy etc) visually
real battery indicator to actually show the battery percentage because we have on board BMS i guess
A coded screensaver animation of robotic eye that is being used everywhere
*/