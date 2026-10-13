#include <stdio.h>
#include <string.h>

#include "cricket.h"
#include "onboarding.h"

#include "../../config/sim_config.h"

static bool s_ready;

// No real network in the simulator - just a canned live score so the
// cricket face has something to render. Gated on the "cricket" interest
// bit exactly like the real fetch, so toggling SIM_DEV_INTERESTS in
// config/sim_config.h behaves the same way it would on real hardware.
void cricket_start(void)
{
    s_ready = (onboarding_get_interests() & ONBOARD_INTEREST_CRICKET) != 0;
}

bool cricket_get(cricket_score_t *out)
{
    if (!s_ready) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->team1, sizeof(out->team1), "%s", SIM_CRICKET_TEAM1);
    snprintf(out->team2, sizeof(out->team2), "%s", SIM_CRICKET_TEAM2);
    snprintf(out->score1, sizeof(out->score1), "%s", SIM_CRICKET_SCORE1);
    snprintf(out->overs1, sizeof(out->overs1), "%s", SIM_CRICKET_OVERS1);
    snprintf(out->score2, sizeof(out->score2), "%s", SIM_CRICKET_SCORE2);
    snprintf(out->overs2, sizeof(out->overs2), "%s", SIM_CRICKET_OVERS2);
    snprintf(out->status, sizeof(out->status), "%s", SIM_CRICKET_STATUS);
    out->valid = true;
    return true;
}
