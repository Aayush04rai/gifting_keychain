#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp_pm.h"

#include "board_hal.h"
#include "power.h"
#include "wifi_sta.h"

static const char *TAG = "power";

static bool s_deep;

// One place for the power-management settings. The CPU never changes speed
// (the display, LED driver and Wi-Fi are all timed against it, and the saving
// from scaling it is small next to light sleep) - the only thing that varies
// is whether the chip may sleep when idle.
static esp_err_t configure(bool light_sleep)
{
    const esp_pm_config_t cfg = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 160,
        .light_sleep_enable = light_sleep,
    };
    return esp_pm_configure(&cfg);
}

void power_init(void)
{
    esp_err_t err = configure(false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "power management unavailable (%s) - deep sleep will only dim the screen",
                 esp_err_to_name(err));
    }
}

void power_deep_enter(void)
{
    if (s_deep) {
        return;
    }
    s_deep = true;

    board_display_sleep(true);
    wifi_sta_set_deep_sleep(true);

    if (usb_serial_jtag_is_connected() && !POWER_LIGHT_SLEEP_WITH_USB) {
        ESP_LOGI(TAG, "deep sleep: screen off, Wi-Fi in low power; CPU kept awake (USB host attached)");
        return;
    }
    esp_err_t err = configure(true);
    ESP_LOGI(TAG, "deep sleep: screen off, Wi-Fi in low power, light sleep %s",
             err == ESP_OK ? "ENABLED" : esp_err_to_name(err));
}

void power_deep_exit(void)
{
    if (!s_deep) {
        return;
    }
    s_deep = false;

    configure(false);
    wifi_sta_set_deep_sleep(false);
    board_display_sleep(false);
    ESP_LOGI(TAG, "woke: light sleep off, Wi-Fi responsive, screen on");
}
