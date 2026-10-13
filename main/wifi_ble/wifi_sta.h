#pragma once

#include <stdbool.h>

// Connects to the recipient's home WiFi using the SSID/password captured
// during onboarding (see onboarding_get_wifi_ssid()/_pass()). No-op if no
// SSID was saved - WiFi was an optional section on the setup form. Call
// once, after onboarding_run_if_needed() and the shared STA netif/driver
// init in main.c, before anything (like main/personalization/) that needs
// internet access. Reconnects automatically on drop.
void wifi_sta_connect(const char *ssid, const char *password);

// True once esp_wifi has actually obtained an IP address on the STA
// interface. Personalization fetches should gate on this rather than
// firing blindly and failing while the radio is still associating.
bool wifi_sta_is_connected(void);

// Trades responsiveness for power while the device is deeply asleep: the
// radio listens far less often but the connection is kept, so it is back
// instantly on waking. Used by power.h.
void wifi_sta_set_deep_sleep(bool deep);
