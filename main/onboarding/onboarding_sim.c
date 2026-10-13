#include <stdlib.h>

#include "onboarding.h"

#include "../../config/sim_config.h"

// No captive portal to simulate on a desktop; pretend onboarding already
// happened with placeholder details so the UI layer (which will eventually
// use these for a personalized welcome/content face) has something
// realistic to render.
void onboarding_run_if_needed(void)
{
}

const char *onboarding_get_name(void)
{
    return SIM_DEV_NAME;
}

const char *onboarding_get_title(void)
{
    return SIM_DEV_TITLE;
}

uint16_t onboarding_get_interests(void)
{
    return SIM_DEV_INTERESTS;
}

// SIM_COMPANION=female|male picks which companion the simulator shows; with
// it unset, SIM_DEV_COMPANION from config/sim_config.h is used.
bool onboarding_get_vibration(void)
{
    const char *v = getenv("SIM_VIBRATION");
    return !v || atoi(v) != 0;
}

const char *onboarding_get_tz(void)
{
    return "IST-5:30";
}

companion_t onboarding_get_companion(void)
{
    const char *v = getenv("SIM_COMPANION");
    if (v) {
        return (v[0] == 'f' || v[0] == 'F') ? COMPANION_FEMALE : COMPANION_MALE;
    }
    return SIM_DEV_COMPANION;
}

const char *onboarding_get_wifi_ssid(void)
{
    return ""; // no real WiFi to join on the desktop simulator
}

const char *onboarding_get_wifi_pass(void)
{
    return "";
}
