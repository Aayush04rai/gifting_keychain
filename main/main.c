#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ble_find.h"
#include "haptic_pattern.h"
#include "board_hal.h"
#include "character.h"
#include "events.h"
#include "gesture.h"
#include "jokes.h"
#include "news.h"
#include "onboarding.h"
#include "power.h"
#include "ui.h"
#include "wifi_sta.h"

static void init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

// The default netif/event loop are one-time setup shared by onboarding's
// temporary AP and this STA config; wifi_scan_run() only toggles
// esp_wifi_start()/stop() per scan, so no connect/reconnect logic lives
// here (yet - see README "Real-hardware readiness").
static void init_wifi_stack(void)
{
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
}

// LEDC starts the backlight channel at 0% duty and the WS2812 driver's
// power-on color is undefined, so both need an explicit initial value -
// there's no longer a settings screen to do it manually.
#define DEFAULT_BACKLIGHT_PCT CHAR_BACKLIGHT_AWAKE_PCT

// Set to 1 to buzz the haptic motor on start-up (a wiring check): three full
// pulses, then 30 / 60 / 100 % strength.
#define HAPTIC_SELFTEST 0

#if HAPTIC_SELFTEST
// Plays every ring pattern in turn, 7 s each, with a 2 s gap, logging its name -
// to feel which one suits. Starts 3 s after power-up and runs once per boot.
static void haptic_selftest_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(3000));
    for (int i = 0; i < HAPTIC_PAT_COUNT; i++) {
        ESP_LOGI("haptic_test", "pattern %d/%d: %s", i + 1, HAPTIC_PAT_COUNT, haptic_pattern_name((haptic_pattern_id_t)i));
        haptic_pattern_start((haptic_pattern_id_t)i);
        vTaskDelay(pdMS_TO_TICKS(7000));
        haptic_pattern_stop();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    ESP_LOGI("haptic_test", "done");
    vTaskDelete(NULL);
}
#endif

void app_main(void)
{
    // Brings up the display (and registers it with LVGL), backlight, WS2812
    // LED, IMU, primary button, and battery ADC. See main/board/board_esp32.c.
    board_init();
    board_set_backlight(DEFAULT_BACKLIGHT_PCT);
    board_set_led(0, 0, 0);
#if HAPTIC_SELFTEST
    xTaskCreate(haptic_selftest_task, "haptic_test", 3072, NULL, 3, NULL);
#endif

    // Optional microSD with the card artwork (assets/sd/). Non-fatal: without
    // a card the cards simply have no pictures.
    board_sd_mount();
    sd_upload_start();

    // Power management on (but not sleeping): deep sleep, which turns on
    // light sleep, comes later and only while the character is fast asleep.
    power_init();

    init_nvs();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Blocks (showing its own setup screen) until the recipient submits the
    // web form, but only on a never-before-onboarded device; a no-op
    // otherwise. Brings its own temporary AP up and back down again, so
    // init_wifi_stack() below always starts from a clean WiFi state.
    onboarding_run_if_needed();

    init_wifi_stack();
    wifi_sta_connect(onboarding_get_wifi_ssid(), onboarding_get_wifi_pass());
    news_start();
    jokes_start();

    // Find my keychain (Bluetooth LE). Needs NVS (bonds) and starts after Wi-Fi so
    // the two radios are set up together.
    ble_find_start();

    // LVGL calls must be serialized with the display's own update loop.
    if (board_lock(0)) {
        ui_create();
        board_unlock();
    }

    events_start();

    gesture_start();
}
