#pragma once

#include <stdbool.h>

// Short, clean, corporate/tech one-liners. Two sources, one front door:
//
//  - A curated list compiled into the firmware (jokes_core.c): hand-written,
//    always available, offline, and safe by construction. This is the
//    backbone.
//  - A small pool topped up from JokeAPI (jokes_esp32.c) in safe mode, for
//    variety. Anything from the network is length-limited, converted to
//    plain ASCII and run through a word filter before it is allowed in, and
//    it is only ever *added* to the curated list - if the network never
//    answers, nothing changes.
void jokes_start(void);

// Returns a joke to show. Never NULL. Never repeats a joke until every one in
// the list has been used once (shuffle-bag), and mixes in an API joke roughly
// one time in three when one is waiting. The pointer stays valid until the
// next call.
const char *joke_next(void);

// For the network top-up: offers a joke from outside. Rejected (returns
// false) if it is too long, empty, or fails the word filter; otherwise it is
// copied into the waiting pool.
bool jokes_offer_external(const char *joke);
