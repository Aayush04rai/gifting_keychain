#pragma once

// Personal free-tier API keys for the personalization features under
// main/personalization/. Each one requires its own free signup - paste
// your key below once you have it. Nothing in this repo calls out to any
// of these services without a real key filled in here.
//
// CricketData.org (formerly CricAPI) - https://cricketdata.org
// Free plan: 100 requests/day, no expiry, no credit card required.
// Sign up, grab your key from the member area, and paste it below. Used by
// main/personalization/cricket_esp32.c for the live cricket score face,
// polling every 15 minutes (~96 requests/day, safely under the free cap)
// whenever the recipient checked "cricket" during onboarding and the
// device has joined its home WiFi.
#define CRICKET_API_KEY "PASTE_YOUR_CRICKETDATA_ORG_KEY_HERE"

// GNews (https://gnews.io) - headline source for the news reactions in
// main/events/ and main/personalization/news_esp32.c.
// Free plan: 100 requests/day, no credit card. Sign up, copy the API key from
// the dashboard and paste it between the quotes below. Left empty, the news
// feature simply stays off (jokes and everything else keep working).
// The firmware spends at most ~80 requests/day regardless of how many topics
// the recipient picked, so it stays under the free cap.
#define GNEWS_API_KEY ""
