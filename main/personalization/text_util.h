#pragma once

#include <stdbool.h>

// The display font only covers printable ASCII, but real headlines and jokes
// arrive as UTF-8 full of curly quotes, en/em dashes, ellipses and the odd
// emoji. Rewrites `s` in place into plain ASCII: typographic punctuation is
// mapped to its ASCII look-alike, no-break spaces become spaces, anything
// else non-ASCII is dropped; control characters and newlines become spaces,
// runs of spaces collapse, and the ends are trimmed. The result is never
// longer than the input.
void text_to_ascii(char *s);

// True if `s` contains any word from a small list of terms that have no
// place on a corporate gift (violence, sex, drink, religion, politics,
// insults, family jokes...). Case-insensitive, whole-substring match, so it
// errs on the side of rejecting. Used to vet anything that did not come from
// our own curated list.
bool text_is_unsafe(const char *s);
