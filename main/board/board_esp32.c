#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_wifi.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/usb_serial_jtag.h"
#include "driver/sdmmc_host.h"
#include "driver/spi_master.h"

#include "button_gpio.h"
#include "iot_button.h"
#include "led_strip.h"

#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "board_hal.h"
#include "haptic_pattern.h"
#include "onboarding.h"
#include "qmi8658.h"
#include "wifi_scan.h"

static const char *TAG = "board_esp32";

#define BACKLIGHT_LEDC_TIMER   LEDC_TIMER_0
#define BACKLIGHT_LEDC_MODE    LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_CHANNEL LEDC_CHANNEL_0
#define BACKLIGHT_LEDC_FREQ_HZ 5000
#define BACKLIGHT_LEDC_RES     LEDC_TIMER_10_BIT

#define BATTERY_ADC_ATTEN    ADC_ATTEN_DB_12
#define BATTERY_ADC_BITWIDTH ADC_BITWIDTH_DEFAULT
// This board senses battery voltage through a resistor divider ahead of the
// ADC pin. Measured: on USB the pin sits at a steady 1.393 V, and the pack's
// full voltage is 4.47 V (multimeter) - so the divider is 1:3.2 (the 3.0 first
// assumed under-read by 7%, which is why the gauge never reached full).
#define BATTERY_DIVIDER_RATIO 3.2f
#define BATTERY_ABSENT_MV     2500 // below this the pin is floating: nothing connected
#define BATTERY_FULL_MV       4470 // the pack's maximum voltage
// Charge curve: voltage on the pin (battery at rest) to percent. Lithium cells
// are far from linear: flat in the middle, steep at the ends. Tuned for a pack
// whose full voltage is 4.47 V; refine with a real discharge log if needed.
static const struct {
    int mv;
    int pct;
} k_battery_curve[] = {
    {3300, 0}, {3500, 5}, {3700, 20}, {3850, 40}, {4000, 60}, {4150, 78}, {4300, 92}, {BATTERY_FULL_MV, 100},
};

static led_strip_handle_t s_led_strip;
static i2c_master_dev_handle_t s_imu_dev;
static bool s_imu_ready;
static adc_oneshot_unit_handle_t s_adc_unit;
static adc_cali_handle_t s_adc_cali;
static bool s_adc_cali_ready;
static button_handle_t s_button;
static volatile bool s_button_clicked;
static volatile bool s_button_long_pressed;

static void init_backlight(void)
{
    const ledc_timer_config_t timer_cfg = {
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .timer_num = BACKLIGHT_LEDC_TIMER,
        .duty_resolution = BACKLIGHT_LEDC_RES,
        .freq_hz = BACKLIGHT_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    const ledc_channel_config_t channel_cfg = {
        .gpio_num = BOARD_LCD_PIN_BL,
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .channel = BACKLIGHT_LEDC_CHANNEL,
        .timer_sel = BACKLIGHT_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_cfg));
}

void board_set_backlight(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    uint32_t max_duty = (1u << BACKLIGHT_LEDC_RES) - 1u;
    uint32_t duty = (max_duty * percent) / 100u;
    ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty);
    ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL);
}

// --- Haptic motor ---------------------------------------------------------
//
// PWM on its own LEDC timer/channel (the backlight owns timer 0 / channel 0).
// Strength is the duty cycle; a one-shot esp_timer ends each pulse.
#define HAPTIC_LEDC_TIMER   LEDC_TIMER_1
#define HAPTIC_LEDC_CHANNEL LEDC_CHANNEL_1
#define HAPTIC_LEDC_FREQ_HZ 2000
#define HAPTIC_LEDC_RES     LEDC_TIMER_8_BIT

static esp_timer_handle_t s_haptic_timer;

static void haptic_set_duty(uint32_t duty)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, HAPTIC_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, HAPTIC_LEDC_CHANNEL);
}

static void haptic_timer_cb(void *arg)
{
    haptic_set_duty(0);
}

static void init_haptic(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = HAPTIC_LEDC_TIMER,
        .duty_resolution = HAPTIC_LEDC_RES,
        .freq_hz = HAPTIC_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    const ledc_channel_config_t channel = {
        .gpio_num = BOARD_HAPTIC_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = HAPTIC_LEDC_CHANNEL,
        .timer_sel = HAPTIC_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
    const esp_timer_create_args_t args = {.callback = haptic_timer_cb, .name = "haptic"};
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_haptic_timer));
}

void board_haptic_pulse(uint16_t ms, uint8_t strength_pct)
{
    if (!s_haptic_timer || ms == 0) {
        return;
    }
    if (ms > 2000) {
        ms = 2000;
    }
    if (strength_pct < 10) {
        strength_pct = 10;
    }
    if (strength_pct > 100) {
        strength_pct = 100;
    }
    esp_timer_stop(s_haptic_timer); // a new pulse replaces one in progress
    haptic_set_duty((255u * strength_pct) / 100u);
    esp_timer_start_once(s_haptic_timer, (uint64_t)ms * 1000u);
}

void board_haptic_feedback(board_haptic_kind_t kind)
{
    if (!onboarding_get_vibration() || haptic_pattern_looping()) {
        return;
    }
    // Never a flurry: a tilt hovering on a boundary must not buzz continuously.
    static uint32_t s_last_ms;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (s_last_ms && now - s_last_ms < 200) {
        return;
    }
    s_last_ms = now;
    haptic_pattern_start(kind == BOARD_HAPTIC_TICK ? HAPTIC_PAT_TICK : HAPTIC_PAT_NOTIFY);
}

static esp_lcd_panel_handle_t s_panel;
static bool s_sd_mounted;

bool board_sd_mount(void)
{
    if (s_sd_mounted) {
        return true;
    }

    // 4-bit SDMMC. The pins go through the GPIO matrix, so they are given
    // explicitly rather than assumed (config/board_config.h).
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT; // 20 MHz: plenty for small image files
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = BOARD_SD_PIN_CLK;
    slot.cmd = BOARD_SD_PIN_CMD;
    slot.d0 = BOARD_SD_PIN_D0;
    slot.d1 = BOARD_SD_PIN_D1;
    slot.d2 = BOARD_SD_PIN_D2;
    slot.d3 = BOARD_SD_PIN_D3;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    const esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false, // never wipe someone's card
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_card_t *card = NULL;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &mount_cfg, &card);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "no usable SD card (%s) - the UI will use its built-in graphics", esp_err_to_name(ret));
        return false;
    }
    s_sd_mounted = true;
    ESP_LOGI(TAG, "SD card mounted: %s, %llu MB", card->cid.name,
             ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
    return true;
}

static uint32_t lvgl_tick_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void board_display_sleep(bool sleep)
{
    if (!s_panel) {
        return;
    }
    if (sleep) {
        board_set_backlight(0);
        esp_lcd_panel_disp_sleep(s_panel, true);
    } else {
        esp_lcd_panel_disp_sleep(s_panel, false); // includes the controller's wake-up delay
    }
}

// Brings up the SPI bus, ST7789 panel, and wires it into LVGL via
// esp_lvgl_port. No vendor BSP exists for this board, so this is the
// direct ESP-IDF equivalent of what such a BSP would normally wrap.
static lv_display_t *init_display(void)
{
    const spi_bus_config_t bus_cfg = {
        .mosi_io_num = BOARD_LCD_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = BOARD_LCD_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_NATIVE_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = BOARD_LCD_PIN_CS,
        .dc_gpio_num = BOARD_LCD_PIN_DC,
        .spi_mode = 0,
        .pclk_hz = BOARD_LCD_SPI_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BOARD_LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_cfg, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    // Standard ST7789 quirk: colors come out inverted without this.
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, BOARD_LCD_OFFSET_X, BOARD_LCD_OFFSET_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    s_panel = panel_handle;

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    // esp_lvgl_port feeds LVGL's clock from a periodic timer that fires every
    // few milliseconds, forever. That alone would keep the chip from ever
    // reaching light sleep. LVGL 9 can read time directly instead, so stop
    // that timer and give LVGL the system clock. (lvgl_port_stop() also
    // disables LVGL's own timers, so they are switched straight back on.)
    if (lvgl_port_lock(0)) {
        lvgl_port_stop();
        lv_tick_set_cb(lvgl_tick_ms);
        lv_timer_enable(true);
        lvgl_port_unlock();
    }

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = BOARD_LCD_NATIVE_H_RES * 40,
        .double_buffer = true,
        .hres = BOARD_LCD_NATIVE_H_RES,
        .vres = BOARD_LCD_NATIVE_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
            .sw_rotate = true,
        },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (disp) {
        // Panel is native portrait (172x320); rotate to the landscape
        // working area ui.c is written for (config/board_config.h).
        lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
    }
    return disp;
}

static void init_led(void)
{
    const led_strip_config_t strip_cfg = {
        .strip_gpio_num = BOARD_LED_PIN_DATA,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };
    const led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags = {
            .with_dma = false,
        },
    };
    esp_err_t ret = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WS2812 init failed: %s", esp_err_to_name(ret));
        s_led_strip = NULL;
    }
}

void board_set_led(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_led_strip) {
        return;
    }
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
}

static void init_imu(void)
{
    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1,
        .sda_io_num = BOARD_IMU_PIN_SDA,
        .scl_io_num = BOARD_IMU_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &bus);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C bus init failed: %s", esp_err_to_name(ret));
        return;
    }

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BOARD_IMU_I2C_ADDR,
        .scl_speed_hz = BOARD_IMU_I2C_HZ,
    };
    ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_imu_dev);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "IMU device add failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = qmi8658_init(s_imu_dev);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "QMI8658 init failed: %s", esp_err_to_name(ret));
        return;
    }
    s_imu_ready = true;
}

void board_get_accel(float *x, float *y, float *z)
{
    if (!s_imu_ready || qmi8658_read_accel(s_imu_dev, x, y, z) != ESP_OK) {
        *x = 0;
        *y = 0;
        *z = 1.0f; // report "flat" if the IMU isn't available
    }
}

static void button_click_cb(void *button_handle, void *usr_data)
{
    (void)button_handle;
    (void)usr_data;
    s_button_clicked = true;
}

static void button_long_cb(void *button_handle, void *usr_data)
{
    (void)button_handle;
    (void)usr_data;
    s_button_long_pressed = true;
}

static void init_button(void)
{
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t gpio_cfg = {
        .gpio_num = BOARD_BUTTON_PIN_BOOT,
        .active_level = 0,
        // Without this the driver scans the pin from a timer every few
        // milliseconds, forever, which would keep the chip from light-sleeping.
        // With it the timer only runs while the button is actually pressed.
        .enable_power_save = true,
    };
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &s_button);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Button init failed: %s", esp_err_to_name(ret));
        return;
    }
    iot_button_register_cb(s_button, BUTTON_SINGLE_CLICK, NULL, button_click_cb, NULL);
    // Fires once while the button is still held (default threshold 1.5 s); a
    // press that long never produces a single click on release.
    iot_button_register_cb(s_button, BUTTON_LONG_PRESS_START, NULL, button_long_cb, NULL);
}

bool board_button_clicked(void)
{
    if (!s_button_clicked) {
        return false;
    }
    s_button_clicked = false;
    return true;
}

bool board_button_long_pressed(void)
{
    if (!s_button_long_pressed) {
        return false;
    }
    s_button_long_pressed = false;
    return true;
}

static void init_battery_adc(void)
{
    const adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = BOARD_BATTERY_ADC_UNIT,
    };
    esp_err_t ret = adc_oneshot_new_unit(&unit_cfg, &s_adc_unit);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC init failed: %s", esp_err_to_name(ret));
        return;
    }

    const adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = BATTERY_ADC_BITWIDTH,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_unit, BOARD_BATTERY_ADC_CHANNEL, &chan_cfg));

    const adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = BOARD_BATTERY_ADC_UNIT,
        .chan = BOARD_BATTERY_ADC_CHANNEL,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = BATTERY_ADC_BITWIDTH,
    };
    s_adc_cali_ready = (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_adc_cali) == ESP_OK);
}

static int battery_read_mv(void)
{
    if (!s_adc_unit) {
        return -1;
    }
    // Average a handful of reads: the ADC wobbles by a few mV.
    int sum = 0, n = 0;
    for (int i = 0; i < 8; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_adc_unit, BOARD_BATTERY_ADC_CHANNEL, &raw) == ESP_OK) {
            int pin_mv = raw;
            if (s_adc_cali_ready) {
                adc_cali_raw_to_voltage(s_adc_cali, raw, &pin_mv);
            }
            sum += pin_mv;
            n++;
        }
    }
    if (n == 0) {
        return -1;
    }
    return (int)((float)sum / n * BATTERY_DIVIDER_RATIO * BOARD_BATTERY_ADC_CAL_FACTOR);
}

static bool s_charging;

// Decides whether the battery is being charged. There is no charge-status pin and
// no VBUS sense, so:
//  - A computer's USB link (usb_serial_jtag_is_connected()) is authoritative:
//    charging while it is there, and the moment it disappears the cable has been
//    pulled - charging stops at once.
//  - A wall charger has no data link, so it is inferred from the voltage on the
//    pin stepping up when it is plugged in (the pin jumps when charge current
//    starts to flow) and stepping down, or sagging back from the level it held,
//    when it is removed.
// The voltage LEVEL is deliberately not used: this pack rests at 4.47 V when full,
// which is the same voltage the pin shows on a charger. A wall charger feeding a
// cell that is already full causes almost no step, so it may go unnoticed; and a
// load spike (Wi-Fi/Bluetooth burst) sags the pin and its recovery must not
// be mistaken for a plug-in.
#define BATTERY_STEP_MV     40
#define BATTERY_SAG_MV      30
#define BATTERY_RECOVER_MS  60000

static void battery_update_charging(int mv)
{
    static int s_last_mv;
    static int s_peak_mv;
    static int s_drop_from_mv;
    static uint32_t s_drop_ms;
    static bool s_prev_usb;

    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    bool usb = usb_serial_jtag_is_connected();

    if (usb) {
        s_charging = true;
        s_peak_mv = mv;
    } else {
        if (s_prev_usb) {
            s_charging = false; // the cable was pulled
        } else if (s_last_mv > 0) {
            int d = mv - s_last_mv;
            bool recovering = s_drop_ms && now - s_drop_ms < BATTERY_RECOVER_MS && mv >= s_drop_from_mv - 25;
            if (d >= BATTERY_STEP_MV && !recovering) {
                s_charging = true; // a charger came on
                s_peak_mv = mv;
            } else if (d <= -BATTERY_STEP_MV) {
                s_charging = false; // it went away (or a load sagged the pin: either way, not "charging")
                s_drop_from_mv = s_last_mv;
                s_drop_ms = now;
            } else if (s_charging) {
                if (mv > s_peak_mv) {
                    s_peak_mv = mv;
                } else if (mv <= s_peak_mv - BATTERY_SAG_MV) {
                    s_charging = false; // fell back from the charger's level
                }
            }
        }
    }
    s_prev_usb = usb;
    s_last_mv = mv;

    // One line every 20 s, so the readings can be compared with a multimeter.
    static int64_t s_last_log_us;
    int64_t now_us = esp_timer_get_time();
    if (now_us - s_last_log_us > 20 * 1000000LL) {
        s_last_log_us = now_us;
        ESP_LOGI(TAG, "battery %d mV usb=%d charging=%d", mv, (int)usb, (int)s_charging);
    }
}

int board_get_battery_pct(void)
{
    int mv = battery_read_mv();
    if (mv < 0) {
        return -1;
    }
    battery_update_charging(mv);
    if (mv < BATTERY_ABSENT_MV) {
        s_charging = false;
        return -1; // the pin is floating: no battery connected
    }
    if (mv <= k_battery_curve[0].mv) {
        return 0;
    }
    const int n = (int)(sizeof(k_battery_curve) / sizeof(k_battery_curve[0]));
    for (int i = 1; i < n; i++) {
        if (mv <= k_battery_curve[i].mv) {
            int lo_mv = k_battery_curve[i - 1].mv, hi_mv = k_battery_curve[i].mv;
            int lo_p = k_battery_curve[i - 1].pct, hi_p = k_battery_curve[i].pct;
            return lo_p + (mv - lo_mv) * (hi_p - lo_p) / (hi_mv - lo_mv);
        }
    }
    return 100;
}

bool board_battery_charging(void)
{
    return s_charging;
}

bool board_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void board_unlock(void)
{
    lvgl_port_unlock();
}

typedef struct {
    void (*fn)(void *ctx);
    void *ctx;
} async_job_t;

static void async_task_trampoline(void *arg)
{
    async_job_t *job = arg;
    job->fn(job->ctx);
    vPortFree(job);
    vTaskDelete(NULL);
}

void board_run_async(void (*fn)(void *ctx), void *ctx)
{
    async_job_t *job = pvPortMalloc(sizeof(async_job_t));
    if (!job) {
        return;
    }
    job->fn = fn;
    job->ctx = ctx;
    if (xTaskCreate(async_task_trampoline, "board_async", 4096, job, 5, NULL) != pdPASS) {
        vPortFree(job);
    }
}

uint32_t board_get_free_heap_kb(void)
{
    return esp_get_free_heap_size() / 1024;
}

uint32_t board_get_uptime_seconds(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000000);
}

int board_get_wifi_rssi(void)
{
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        return 0; // not associated (or the driver isn't up yet)
    }
    return ap.rssi;
}

void board_wifi_scan(board_wifi_ap_t *out_aps, uint16_t *out_count, uint16_t max_aps)
{
    wifi_ap_record_t records[WIFI_SCAN_MAX_APS];
    uint16_t count = 0;
    uint16_t max_records = max_aps > WIFI_SCAN_MAX_APS ? WIFI_SCAN_MAX_APS : max_aps;

    wifi_scan_run(records, &count, max_records);

    for (uint16_t i = 0; i < count; i++) {
        snprintf(out_aps[i].ssid, sizeof(out_aps[i].ssid), "%s",
                 records[i].ssid[0] ? (const char *)records[i].ssid : "(hidden)");
        out_aps[i].rssi = records[i].rssi;
    }
    *out_count = count;
}

// Individual init_* helpers log and continue on failure for non-critical
// peripherals (IMU/button/ADC/LED) so one missing/faulty part doesn't
// prevent the display and UI from coming up.
void board_init(void)
{
    init_backlight();
    if (!init_display()) {
        ESP_LOGE(TAG, "Display init failed - halting");
        return;
    }
    init_led();
    init_haptic();
    init_imu();
    init_button();
    init_battery_adc();
}
