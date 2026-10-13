#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "onboarding.h"
#include "wifi_sta.h"

static const char *TAG = "wifi_sta";
static volatile bool s_connected;
static bool s_sntp_started;

// POSIX TZ string; faces call localtime(), so this is the one place that
// decides what "local" means. India Standard Time (UTC+5:30, no DST).

static void time_synced_cb(struct timeval *tv)
{
    (void)tv;
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_info);
    ESP_LOGI(TAG, "time synced: %s (%s)", buf, onboarding_get_tz());
}

// Independent of SNTP's own callback: polls the system clock once a second
// and logs the moment it first holds a plausible date (anything before 2024
// means "still counting from boot"), then stops.
static esp_timer_handle_t s_clock_timer;
static int s_clock_ticks;

static void clock_watch_cb(void *arg)
{
    (void)arg;
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    s_clock_ticks++;
    if (tm_info.tm_year + 1900 >= 2024) {
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_info);
        ESP_LOGI(TAG, "clock valid after %d s: %s (%s)", s_clock_ticks, buf, onboarding_get_tz());
        esp_timer_stop(s_clock_timer);
    } else if (s_clock_ticks >= 30) {
        ESP_LOGW(TAG, "clock still not set 30 s after connecting");
        esp_timer_stop(s_clock_timer);
    }
}

static void watch_clock(void)
{
    if (!s_clock_timer) {
        const esp_timer_create_args_t args = {.callback = clock_watch_cb, .name = "clock_watch"};
        ESP_ERROR_CHECK(esp_timer_create(&args, &s_clock_timer));
    }
    esp_timer_stop(s_clock_timer); // ESP_ERR_INVALID_STATE if idle; fine
    s_clock_ticks = 0;
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_clock_timer, 1000000));
}

// Called on every got-IP, so the clock is corrected the moment the board
// (re)joins a network rather than waiting for SNTP's own hourly poll.
static void start_time_sync(void)
{
    if (s_sntp_started) {
        esp_sntp_restart();
        return;
    }
    s_sntp_started = true;

    setenv("TZ", onboarding_get_tz(), 1);
    tzset();

    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    cfg.sync_cb = time_synced_cb;
    // Poll every 15 min instead of the default hour: cheap, and a watch-like
    // device shouldn't drift visibly between syncs.
    esp_sntp_set_sync_interval(15 * 60 * 1000);
    ESP_ERROR_CHECK(esp_netif_sntp_init(&cfg));
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        ESP_LOGW(TAG, "disconnected, retrying");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_connected = true;
        ESP_LOGI(TAG, "connected");
        start_time_sync();
        watch_clock();
    }
}

void wifi_sta_connect(const char *ssid, const char *password)
{
    if (!ssid || !ssid[0]) {
        ESP_LOGI(TAG, "no WiFi credentials saved during onboarding, skipping STA connect");
        return;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    wifi_config_t sta_config = {0};
    snprintf((char *)sta_config.sta.ssid, sizeof(sta_config.sta.ssid), "%s", ssid);
    snprintf((char *)sta_config.sta.password, sizeof(sta_config.sta.password), "%s", password ? password : "");
    // Only used while in deep sleep (WIFI_PS_MAX_MODEM): wake for the access
    // point's beacon every 10th beacon (about a second) instead of every one,
    // which is what lets the radio and the chip rest between them.
    sta_config.sta.listen_interval = 10;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();
}

bool wifi_sta_is_connected(void)
{
    return s_connected;
}

void wifi_sta_set_deep_sleep(bool deep)
{
    // Stays associated either way, so the connection is still there the
    // moment the device wakes. MIN_MODEM wakes for every beacon (responsive);
    // MAX_MODEM wakes only every listen_interval beacons (frugal, slower to
    // receive). Harmless if the radio was never started.
    esp_wifi_set_ps(deep ? WIFI_PS_MAX_MODEM : WIFI_PS_MIN_MODEM);
}
