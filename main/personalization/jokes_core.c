#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"

#include "jokes.h"
#include "text_util.h"

// Hand-written for this product. Rules they all follow: workplace or
// technology themes, self-aware rather than mean, nothing about people's
// bodies, families, politics, religion or health, nothing a senior person
// would wince at in front of a colleague. Short enough for the card (about
// three lines at 20 px, ~75 characters).
static const char *const k_jokes[] = {
    "404: Work-life balance not found.",
    "Another meeting? Bold choice.",
    "Your calendar looks aggressive.",
    "Deploying optimism...",
    "Meeting detected. Initiating emotional support.",
    "Your battery is doing better than your sleep schedule.",
    "I checked your schedule. We need to talk.",
    "You survived the meeting.",
    "This meeting could have been an email. Just saying.",
    "Rebooting patience. Please hold.",
    "Have you tried turning the quarterly plan off and on again?",
    "Synergy levels: suspiciously high today.",
    "Running on coffee and unresolved tickets.",
    "Scope creep detected. Defensive measures engaged.",
    "Roadmap updated. Reality unchanged.",
    "It works on my machine. Ship it?",
    "Your inbox called. It wants a break too.",
    "Reply-all is a lifestyle, not a strategy.",
    "Agile: because 'we have no plan' sounds worse.",
    "Legacy code: someone else's confident decision.",
    "Two hard problems: naming things, cache invalidation, off-by-one errors.",
    "Latency is just patience with extra steps.",
    "That deadline is more of a suggestion with ambition.",
    "Priority one: everything. Priority two: also everything.",
    "Standup: where we stand, nothing moves, everyone nods.",
    "Technical debt: the interest rate is brutal.",
    "Your five-minute call is now in its fifth hour.",
    "Today's forecast: 90% chance of follow-ups.",
    "The mute button: humanity's greatest invention.",
    "Let's circle back. Shall I draw the circle?",
    "Out of office is a state of mind.",
    "Cache cleared. Mind freshly empty.",
    "Debugging: being the detective in a crime you also committed.",
    "Documentation is a love letter to your future self.",
    "The cloud is just someone else's computer, on call.",
    "Please hold. Your productivity is important to us.",
    "Bandwidth low. Opinions: plentiful.",
    "Version 2.0 will fix it. Version 1.0 said that too.",
    "Your to-do list has to-do lists.",
    "Quick question: famous last words.",
    "Calendar full. Optimism available.",
    "Thinking outside the box. The box has a meeting.",
    "Failing fast. Succeeding eventually.",
    "Be the change you want to see in the commit history.",
    "Compiling confidence... 3 warnings, 0 errors.",
    "Zero bugs. Possibly zero tests.",
    "Passwords should be long, strong, and not 'Winter2024!'.",
    "Phishing tip: if it urgently needs gift cards, it's not me.",
    "Two-factor authentication: because one regret isn't enough.",
    "Backups: the art of caring before it hurts.",
    "Machine learning: teaching computers to be confidently wrong.",
    "Neural network, meet unsolved quarterly goals.",
    "May your builds be green and your Mondays short.",
    "Hydration status: unknown. Coffee status: critical.",
    "Your focus time called. It went to voicemail.",
    "Strategy meeting: PowerPoint's yearly reunion.",
    "KPIs are green. Don't ask which ones.",
    "Alignment achieved. Direction pending.",
    "I'm only a keychain, but even I can see the scope creep.",
    "I hold your keys. Also, apparently, your attention.",
    "Fun fact: 'sync' is just 'meeting' with extra steps.",
    "Today's goal: fewer tabs, fewer regrets.",
    "Inbox zero is a myth. Inbox manageable is a gift.",
    "Ctrl+Z works on documents. Sadly not on decisions.",
    "Please enjoy this brief moment of nothing scheduled.",
    "System update available. Your weekend was not included.",
    "Plot twist: the meeting ended on time.",
    "Agenda item one: finding the agenda.",
    "The only thing scaling faster than the startup is the Slack channel list.",
    "Your Wi-Fi is fine. It's the meeting that's frozen.",
    "Estimating software: the art of being precisely wrong.",
    "Dark mode: because your late-night ideas deserve ambience.",
    "Pro tip: 'per my last email' is a love language.",
    "Breaking: local team agrees to a clear owner.",
    "Moving fast and fixing things, in that order, ideally.",
};

#define JOKE_COUNT (sizeof(k_jokes) / sizeof(k_jokes[0]))

// --- shuffle-bag over the curated list ------------------------------------

static uint8_t s_bag[JOKE_COUNT];
static size_t s_bag_left;

static void refill_bag(int avoid_first)
{
    for (size_t i = 0; i < JOKE_COUNT; i++) {
        s_bag[i] = (uint8_t)i;
    }
    for (size_t i = JOKE_COUNT - 1; i > 0; i--) { // Fisher-Yates
        size_t j = lv_rand(0, (uint32_t)i);
        uint8_t t = s_bag[i];
        s_bag[i] = s_bag[j];
        s_bag[j] = t;
    }
    // Don't open the new round with the joke that closed the last one.
    if (avoid_first >= 0 && s_bag[JOKE_COUNT - 1] == avoid_first) {
        uint8_t t = s_bag[0];
        s_bag[0] = s_bag[JOKE_COUNT - 1];
        s_bag[JOKE_COUNT - 1] = t;
    }
    s_bag_left = JOKE_COUNT;
}

// --- pool of vetted jokes from the network --------------------------------

#define EXTERNAL_POOL 8
#define EXTERNAL_MAX_LEN 96

static char s_pool[EXTERNAL_POOL][EXTERNAL_MAX_LEN + 1];
static size_t s_pool_count;
static char s_current[EXTERNAL_MAX_LEN + 1]; // what joke_next() last handed out from the pool
static int s_last_curated = -1;

bool jokes_offer_external(const char *joke)
{
    if (!joke || s_pool_count >= EXTERNAL_POOL) {
        return false;
    }
    char tmp[EXTERNAL_MAX_LEN + 8];
    snprintf(tmp, sizeof(tmp), "%s", joke);
    text_to_ascii(tmp);

    size_t len = strlen(tmp);
    if (len < 15 || len > EXTERNAL_MAX_LEN || text_is_unsafe(tmp)) {
        return false;
    }
    for (size_t i = 0; i < s_pool_count; i++) {
        if (strcmp(s_pool[i], tmp) == 0) {
            return false;
        }
    }
    memcpy(s_pool[s_pool_count++], tmp, len + 1);
    return true;
}

const char *joke_next(void)
{
    // One time in three, use a fresh network joke if one is waiting.
    if (s_pool_count > 0 && lv_rand(0, 2) == 0) {
        memcpy(s_current, s_pool[0], sizeof(s_current));
        memmove(s_pool[0], s_pool[1], (s_pool_count - 1) * sizeof(s_pool[0]));
        s_pool_count--;
        return s_current;
    }
    if (s_bag_left == 0) {
        refill_bag(s_last_curated);
    }
    s_last_curated = s_bag[--s_bag_left];
    return k_jokes[s_last_curated];
}
