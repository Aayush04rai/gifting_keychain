#include <ctype.h>
#include <string.h>

#include "text_util.h"

// UTF-8 sequences worth keeping, as their ASCII equivalent. Anything else
// multi-byte is dropped.
typedef struct {
    const char *utf8;
    const char *ascii;
} map_t;

static const map_t k_map[] = {
    {"\xE2\x80\x98", "'"},  // left single quote
    {"\xE2\x80\x99", "'"},  // right single quote / apostrophe
    {"\xE2\x80\x9C", "\""}, // left double quote
    {"\xE2\x80\x9D", "\""}, // right double quote
    {"\xE2\x80\x93", "-"},  // en dash
    {"\xE2\x80\x94", "-"},  // em dash
    {"\xE2\x80\xA6", "..."},// ellipsis
    {"\xC2\xA0", " "},      // no-break space
    {"\xE2\x80\x89", " "},  // thin space
};

void text_to_ascii(char *s)
{
    char *out = s;
    const unsigned char *in = (const unsigned char *)s;
    bool last_space = true; // also trims leading spaces

    while (*in) {
        const char *rep = NULL;
        size_t consumed = 1;

        if (*in < 0x80) {
            char c = (char)*in;
            rep = (c < 0x20 || c == 0x7f) ? " " : NULL;
            if (!rep) {
                // plain printable ASCII: emit directly below
                if (c == ' ') {
                    if (!last_space) {
                        *out++ = ' ';
                    }
                    last_space = true;
                } else {
                    *out++ = c;
                    last_space = false;
                }
                in++;
                continue;
            }
        } else {
            // Work out how long this UTF-8 sequence is, then try the table.
            size_t n = (*in >= 0xF0) ? 4 : (*in >= 0xE0) ? 3 : (*in >= 0xC0) ? 2 : 1;
            consumed = n;
            for (size_t i = 0; i < sizeof(k_map) / sizeof(k_map[0]); i++) {
                size_t l = strlen(k_map[i].utf8);
                if (l == n && memcmp(in, k_map[i].utf8, l) == 0) {
                    rep = k_map[i].ascii;
                    break;
                }
            }
            // Don't run past the end of a truncated sequence.
            for (size_t i = 1; i < consumed; i++) {
                if (in[i] == '\0') {
                    consumed = i;
                    break;
                }
            }
        }

        if (rep) {
            for (const char *r = rep; *r; r++) {
                if (*r == ' ') {
                    if (!last_space) {
                        *out++ = ' ';
                    }
                    last_space = true;
                } else {
                    *out++ = *r;
                    last_space = false;
                }
            }
        }
        in += consumed;
    }

    if (out > s && out[-1] == ' ') {
        out--;
    }
    *out = '\0';
}

// Whole words that disqualify a line. Matched as complete words, so
// "deadline", "skill" and "hello" are fine while "dead", "kill" and "hell"
// are not.
static const char *const k_deny_words[] = {
    // violence / death
    "kill", "kills", "killed", "killing", "murder", "suicide", "dead", "death", "die", "dies", "died",
    "shoot", "shot", "gun", "guns", "bomb", "terror", "rape", "abuse", "assault", "slave",
    // sex / body / drink / drugs
    "sex", "sexy", "porn", "naked", "nude", "drunk", "beer", "vodka", "wine", "alcohol", "hangover",
    "weed", "drug", "drugs", "cocaine", "fat", "ugly", "boob", "boobs", "penis", "dick", "ass", "butt",
    // insults
    "stupid", "idiot", "dumb", "moron", "loser", "hate",
    // identity, religion, politics
    "god", "jesus", "allah", "church", "muslim", "jew", "jews", "christian", "hindu", "gay", "blonde",
    "blondes", "race", "trump", "biden", "putin", "election", "democrat", "republican",
    // family / relationships (easy to land badly)
    "wife", "husband", "girlfriend", "boyfriend", "mom", "mother", "mama", "divorce",
    // profanity
    "damn", "hell", "crap", "shit", "fuck", "bitch", "bastard",
};

// Word stems: anything starting with these is rejected.
static const char *const k_deny_stems[] = {
    "religio", "politic", "immigra", "pregnan", "feminis", "suicid", "sexual", "sexis", "racis",
};

static bool word_is_denied(const char *w)
{
    for (size_t i = 0; i < sizeof(k_deny_words) / sizeof(k_deny_words[0]); i++) {
        if (strcmp(w, k_deny_words[i]) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < sizeof(k_deny_stems) / sizeof(k_deny_stems[0]); i++) {
        if (strncmp(w, k_deny_stems[i], strlen(k_deny_stems[i])) == 0) {
            return true;
        }
    }
    return false;
}

bool text_is_unsafe(const char *s)
{
    char word[24];
    size_t n = 0;

    for (;; s++) {
        unsigned char c = (unsigned char)*s;
        if (isalpha(c)) {
            if (n < sizeof(word) - 1) {
                word[n++] = (char)tolower(c);
            }
            continue;
        }
        if (n > 0) {
            word[n] = '\0';
            if (word_is_denied(word)) {
                return true;
            }
            n = 0;
        }
        if (c == '\0') {
            return false;
        }
    }
}
