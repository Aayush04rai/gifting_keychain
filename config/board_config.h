#pragma once

// Central hardware description for the Waveshare ESP32-S3-LCD-1.47B board
// (ESP32-S3R8, ST7789 172x320 panel, QMI8658 IMU, WS2812 LED, LiPo battery
// header - the "B" variant has no touch panel, unlike ESP32-S3-Touch-LCD-1.47).
// Change values here rather than hunting through main/ and sim/ for magic
// numbers.
//
// No vendor ESP-IDF BSP/managed component exists for this board (only an
// Arduino demo), so main/board_esp32.c drives these pins directly against
// ESP-IDF's own esp_lcd/i2c/adc/spi drivers. Pin numbers below are taken
// from the vendor's official Arduino demo firmware
// (ESP32-S3-LCD-1.47B-Demo.zip, LVGL_Arduino example), cross-checked
// against the wiki's interface tables.

// --- ST7789 LCD (SPI) ---
#define BOARD_LCD_PIN_SCLK 40
#define BOARD_LCD_PIN_MOSI 45
#define BOARD_LCD_PIN_CS   42
#define BOARD_LCD_PIN_DC   41
#define BOARD_LCD_PIN_RST  39
#define BOARD_LCD_PIN_BL   46
#define BOARD_LCD_SPI_HZ   (80 * 1000 * 1000)

// Native ST7789 panel resolution (portrait).
#define BOARD_LCD_NATIVE_H_RES 172
#define BOARD_LCD_NATIVE_V_RES 320

// main.c rotates the panel 90 degrees at startup, so ui.c's actual working
// area - and what the simulator window must match - is this rotated,
// landscape size.
#define BOARD_LCD_H_RES BOARD_LCD_NATIVE_V_RES
#define BOARD_LCD_V_RES BOARD_LCD_NATIVE_H_RES

// Panel is mounted with a column offset on this board (matches vendor demo).
#define BOARD_LCD_OFFSET_X 34
#define BOARD_LCD_OFFSET_Y 0

// --- QMI8658 6-axis IMU (I2C) ---
// No touch panel on this board; tilt is the input method instead (see
// main/gesture.c). Address is the "low" variant used by the vendor demo.
#define BOARD_IMU_PIN_SDA     48
#define BOARD_IMU_PIN_SCL     47
#define BOARD_IMU_I2C_HZ      (400 * 1000)
#define BOARD_IMU_I2C_ADDR    0x6B

// --- Onboard WS2812 RGB LED ---
#define BOARD_LED_PIN_DATA 38

// --- Primary button (BOOT) - the only user-facing button besides RESET ---
#define BOARD_BUTTON_PIN_BOOT 0

// --- Haptic motor (coin vibration motor behind a transistor / MOSFET) ---
// A free GPIO on the header; the pin drives the transistor's gate/base, never
// the motor directly (a motor draws more than a GPIO can give). Change this to
// wherever it is wired.
#define BOARD_HAPTIC_PIN 10

// --- Battery voltage sense (ADC1, resistor-divided) ---
#define BOARD_BATTERY_ADC_UNIT    ADC_UNIT_1
#define BOARD_BATTERY_ADC_CHANNEL ADC_CHANNEL_0 // GPIO1
// Empirical calibration offset from the vendor demo's BAT_Driver.
#define BOARD_BATTERY_ADC_CAL_FACTOR 1.0028f

// --- microSD (4-bit SDMMC) - wiring documented, not yet used by firmware ---
#define BOARD_SD_PIN_CLK 14
#define BOARD_SD_PIN_CMD 15
#define BOARD_SD_PIN_D0  16
#define BOARD_SD_PIN_D1  18
#define BOARD_SD_PIN_D2  17
#define BOARD_SD_PIN_D3  21
