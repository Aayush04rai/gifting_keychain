#include <stdio.h>
#include <string.h>
#include <time.h>

#include "news.h"
#include "news_topics.h"
#include "onboarding.h"
#include "text_util.h"

#include "../../config/sim_config.h"

typedef struct {
    uint16_t bit;
    const char *title;
} sim_headline_t;

static const sim_headline_t k_headlines[] = {SIM_NEWS_LIST};
#define HEADLINE_COUNT (sizeof(k_headlines) / sizeof(k_headlines[0]))

static bool s_started;
static size_t s_next;

static const char *label_for(uint16_t bit)
{
    for (size_t i = 0; i < NEWS_TOPIC_COUNT; i++) {
        if (NEWS_TOPICS[i].bit == bit) {
            return NEWS_TOPICS[i].label;
        }
    }
    return "NEWS";
}

void news_start(void)
{
    s_started = true;
}

bool news_available(void)
{
    return s_started;
}

bool news_take(news_item_t *out)
{
    if (!s_started) {
        return false;
    }
    uint16_t interests = onboarding_get_interests();
    // Walk the canned list round-robin, skipping topics the recipient did not pick.
    for (size_t tried = 0; tried < HEADLINE_COUNT; tried++) {
        const sim_headline_t *h = &k_headlines[s_next++ % HEADLINE_COUNT];
        if (interests & h->bit) {
            out->interest = h->bit;
            snprintf(out->category, sizeof(out->category), "%s", label_for(h->bit));
            snprintf(out->title, sizeof(out->title), "%s", h->title);
            text_to_ascii(out->title);
            snprintf(out->source, sizeof(out->source), "%s", "Daily Wire Desk");
            out->published = (int64_t)time(NULL) - 2 * 3600; // "2h ago"
            return true;
        }
    }
    return false;
}
