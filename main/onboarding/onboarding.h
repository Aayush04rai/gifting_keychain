#pragma once

#include <stdbool.h>
#include <stdint.h>

// First-run setup. On real hardware, if the recipient hasn't been
// onboarded yet (no flag in NVS), this brings up a temporary WiFi access
// point + a small web form (name, title, interests, home WiFi
// SSID/password), shows setup instructions with a scan-to-join QR code on
// the display, and blocks until the form is submitted - then tears the
// setup AP down so normal WiFi (main.c's STA mode) can take over. Call
// once, early in app_main(), after board_init() and NVS init, before the
// normal WiFi stack comes up.
//
// On the desktop simulator there's no captive portal to simulate: this
// returns immediately with a placeholder name and no saved WiFi
// credentials (see config/sim_config.h).
void onboarding_run_if_needed(void);

// The recipient's name captured during onboarding (or the simulator's
// placeholder). Valid only after onboarding_run_if_needed() returns.
const char *onboarding_get_name(void);

// Recipient's title/role, e.g. "Chief Technology Officer" - optional, may
// be an empty string.
const char *onboarding_get_title(void);

// Bitmask of ONBOARD_INTEREST_* the recipient checked on the setup form,
// for a future personalized-content watch face to key off of.
#define ONBOARD_INTEREST_CRICKET  (1u << 0)
#define ONBOARD_INTEREST_F1       (1u << 1)
#define ONBOARD_INTEREST_FOOTBALL (1u << 2)
#define ONBOARD_INTEREST_TECH     (1u << 3)
#define ONBOARD_INTEREST_FINANCE  (1u << 4)
#define ONBOARD_INTEREST_TRAVEL   (1u << 5)
#define ONBOARD_INTEREST_AI       (1u << 6)
#define ONBOARD_INTEREST_ROBOTICS (1u << 7)
#define ONBOARD_INTEREST_SEMI     (1u << 8)
#define ONBOARD_INTEREST_SPACE    (1u << 9)
#define ONBOARD_INTEREST_EV       (1u << 10)
#define ONBOARD_INTEREST_HEALTH   (1u << 11)
#define ONBOARD_INTEREST_DEFENCE  (1u << 12)
#define ONBOARD_INTEREST_CYBER    (1u << 13)
uint16_t onboarding_get_interests(void);

// Which companion the recipient chose on the setup form. It changes the
// character's looks (the female companion has eyelashes) and what it does when
// it has been left alone for a while (see main/character/acts/), but nothing
// else: the colours and lighting are the same for both. Defaults to male for a
// device that was set up before the question existed.
typedef enum {
    COMPANION_MALE = 0,
    COMPANION_FEMALE = 1,
} companion_t;
companion_t onboarding_get_companion(void);

// Whether the haptic motor may buzz for notifications and choices (the "Vibration"
// setting on the setup form). Defaults to on. Find my keychain's ring ignores it:
// a lost keychain has to be able to make itself felt.
bool onboarding_get_vibration(void);

// The recipient's time zone as a POSIX TZ string (e.g. "IST-5:30" or
// "EST5EDT,M3.2.0,M11.1.0"), chosen on the setup form. Defaults to India for a
// device set up before the question existed. Applied by wifi_sta.c once time is
// set.
const char *onboarding_get_tz(void);

// Home WiFi credentials captured on the setup form (optional - may be
// empty strings if the recipient skipped that section, or on the
// simulator, which has no real WiFi to join). Valid only after
// onboarding_run_if_needed() returns.
const char *onboarding_get_wifi_ssid(void);
const char *onboarding_get_wifi_pass(void);
