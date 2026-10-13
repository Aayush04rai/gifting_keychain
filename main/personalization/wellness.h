#pragma once

// Light wellness nudges. Deliberately not a health feature: no measuring, no
// diagnosing, no advice about how the person is doing. Just short, kind
// reminders anyone could use ("Shoulders down.") shown on a card.
//
// Returns the next line to show. Never NULL; never the same line twice in a
// row. The pointer stays valid for the life of the program.
const char *wellness_next(void);

// Replies to the daily check-in, indexed 0 = Good, 1 = Okay, 2 = Rough.
// Supportive and simple; never a judgement or a prescription.
const char *wellness_checkin_reply(int answer);
