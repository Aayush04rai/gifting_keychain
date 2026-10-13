# ESP32-S3-LCD-1.47B LVGL UI — Corporate Gift Keychain

ESP-IDF + LVGL firmware for a keychain-sized ambient device gifted to
CEO/CTO-level recipients, built on the
[Waveshare ESP32-S3-LCD-1.47B](https://www.waveshare.com/product/esp32-related/displays/lcd-spi-qspi/esp32-s3-lcd-1.47b.htm)
(ESP32-S3R8, 172x320 ST7789 SPI LCD, QMI8658 6-axis IMU, WS2812 RGB LED,
3.7V LiPo battery header, 16 MB flash, 8 MB octal PSRAM, WiFi 4 + BLE 5).

**No touch panel on this board** (that's a different SKU,
ESP32-S3-Touch-LCD-1.47) — the IMU is the input device instead: tilting the
keychain left/right swipes between screens, mirroring the vendor's own
"simulated gesture" demo pattern. A BOOT-button click does the same thing
as a fallback/secondary input.

No vendor ESP-IDF BSP exists for this board (only an Arduino demo), so
`main/board/board_esp32.c` drives the ST7789 panel, IMU, LED, button, and
battery ADC directly against ESP-IDF's own `esp_lcd`/`i2c_master`/`adc_oneshot`
drivers plus `espressif/led_strip` and `espressif/button`. Pin numbers came
from the vendor's Arduino demo firmware (`ESP32-S3-LCD-1.47B-Demo.zip`),
cross-checked against the wiki.

## Project layout

`main/` is one ESP-IDF component, organized into subfolders by
functionality (not split into separate components, to keep `REQUIRES`
wiring simple at this size):

```
main/
  main.c            - app_main(): brings up the board, then the UI
  board/            - hardware: board_hal.h (the abstraction), board_esp32.c
                      (real bring-up: display/backlight/LED/IMU/button/battery),
                      qmi8658.c/h (IMU register driver)
  wifi_ble/         - networking: wifi_scan.c/h today; future BLE code lands here
  onboarding/       - first-run setup (see "Onboarding")
  ui/               - ui.c (watch-face gallery/switcher), gesture.c (tilt/button
                      -> navigation), watchface_common.c + watchface_*.c (faces)
config/             - shared hardware/simulator/LVGL config (see below)
sim/                - desktop SDL2 simulator (see "Desktop simulator")
```

Every file in `board/`, `wifi_ble/`, and `ui/` is on the same include path
(`main/CMakeLists.txt`'s `INCLUDE_DIRS`), so `#include "board_hal.h"` etc.
resolve regardless of which subfolder the including file is in.

## Stack

- **ESP-IDF v5.5.5**, target `esp32s3` — toolchain and build system.
- **LVGL 9.5.0** via the Component Manager, wired to the panel through
  [`espressif/esp_lvgl_port`](https://components.espressif.com/components/espressif/esp_lvgl_port).
- **`espressif/led_strip`** — WS2812 RGB LED.
- **`espressif/button`** — BOOT button click/double-click/long-press
  detection.
- All four declared in [`main/idf_component.yml`](main/idf_component.yml),
  downloaded automatically into `managed_components/` on first build.

Full pinout (LCD, IMU I2C, LED, button, battery ADC, microSD) is documented
in [`config/board_config.h`](config/board_config.h) — see **Config layout**.

## Config layout

All tunable/hardware config lives under [`config/`](config/) instead of
being scattered through `main/` and `sim/`:

| File | What it's for |
| --- | --- |
| [`config/board_config.h`](config/board_config.h) | Hardware facts: GPIO pinout (LCD, IMU, LED, button, battery ADC, SD), panel resolution. Shared by `main/board/board_hal.h` on both the real firmware and the simulator. |
| [`config/sim_config.h`](config/sim_config.h) | Simulator-only tunables: window title/zoom, fake WiFi scan delay and AP list, fake free-heap value. Included directly by `sim/main_sim.c` and `sim/board_sim.c`. |
| [`config/lv_conf_sim.h`](config/lv_conf_sim.h) | LVGL's build config for the simulator target (fonts, widgets, the SDL driver). Wired in via `LV_BUILD_CONF_PATH` in `sim/CMakeLists.txt`. |

The real firmware's LVGL config is controlled through ESP-IDF's Kconfig/
`sdkconfig.defaults` instead, and `sdkconfig.defaults`/`main/idf_component.yml`
stay in their ESP-IDF-mandated locations rather than moving into `config/`.

To change a pin, resolution, or simulator constant, edit the one file above
— nothing else in `main/` or `sim/` should hardcode these values.

## Hardware abstraction

[`main/board/board_hal.h`](main/board/board_hal.h) is the only thing
`main/ui/*.c` are written against — no direct `esp_lcd`/FreeRTOS/SDL calls
in any of them — so both compile unchanged for the real board and the
desktop simulator:

- `main/board/board_esp32.c` implements it for real hardware: ST7789
  bring-up + `esp_lvgl_port`, LEDC backlight PWM, WS2812 via `led_strip`,
  QMI8658 IMU (`main/board/qmi8658.c`, register-level I2C driver), BOOT
  button via `espressif/button`, battery voltage via `adc_oneshot` +
  `adc_cali`.
- `sim/board_sim.c` implements it for the simulator: arrow keys fake tilt,
  Space fakes the button click, WiFi scan and battery are mocked (see
  `config/sim_config.h`).

## Onboarding

[`main/onboarding/onboarding.h`](main/onboarding/onboarding.h) declares
`onboarding_run_if_needed()`, called once from `main.c` right after NVS +
the base netif/event loop come up, before the normal WiFi stack starts.

- **`main/onboarding/onboarding_esp32.c`** (real hardware): if an NVS flag
  says onboarding hasn't happened yet, it starts a temporary open WiFi AP
  (`ThinkRobotics Setup`), shows a setup screen with a scan-to-join QR code
  (`WIFI:T:nopass;S:...;;`) and the `192.168.4.1` address, and hosts a tiny
  `esp_http_server` form (name, optional home WiFi SSID/password) at that
  address. Submitting the form saves everything to NVS, shows a "Welcome,
  {name}" screen, tears the AP + HTTP server down, and returns — `main.c`
  then brings up normal STA-mode WiFi as usual. On every later boot, the
  NVS flag makes this a no-op. WiFi credentials are saved but nothing
  currently uses them to auto-connect in STA mode yet (see "Real-hardware
  readiness"); `onboarding_get_name()` exposes the captured name for a
  future personalized-welcome face.
- **`main/onboarding/onboarding_sim.c`** (simulator): no captive portal to
  simulate on a desktop, so this is a no-op that immediately reports the
  placeholder name in `config/sim_config.h`'s `SIM_DEV_NAME`.

## UI

`main/ui/ui.c` is a small watch-face gallery, not a tabbed app:
`main/ui/gesture.c` turns IMU tilt / BOOT-button clicks into
`ui_next_face()`/`ui_prev_face()`, which tears down the current face's
objects+timer (`lv_obj_clean()` on the one shared container +
`lv_timer_delete()`) and builds the next one fresh. No visible tab bar or
chrome — full-screen face, cross-faded by the switch.

Three faces ship today, each its own module, all sharing one visual frame
(`watchface_common.c`'s `watchface_build_frame()` — a single bordered,
rounded card near-fullscreen, per the reference layout: the dial/hero
content in its own rounded box on the left, plain info text on the right,
no floating badges):

| Face | File | Look |
| --- | --- | --- |
| Classic | `main/ui/watchface_classic.c` | Apple Watch "Simple"-inspired analog dial (`lv_scale` + needle lines), date + battery in the info column. |
| Digital | `main/ui/watchface_digital.c` | Bold `HH:MM` in Montserrat 48 with small running seconds, day/date + battery below. |
| Ring | `main/ui/watchface_ring.c` | A once-a-minute sweeping `lv_arc` progress ring around a smaller digital time, battery in the info column. |

Add a new face by writing a `lv_timer_t *my_face_build(lv_obj_t *parent)`
function (build UI, start and return an update timer or `NULL`) and adding
it to the `s_faces[]` array in `main/ui/ui.c`. This is also where a
Canva-designed custom face will slot in once that reference layout is
ready.

## Desktop simulator

`sim/` is a standalone LVGL + SDL2 desktop app (built with MSVC + vcpkg, not
ESP-IDF) that links `main/ui/*.c` unchanged, so you can iterate on the UI
without flashing hardware. (QEMU doesn't support the
ESP32-S3-LCD-1.47B's exact peripheral set either, so this is the fast inner
loop instead.) Controls: **arrow keys** = tilt gesture, **Space** = BOOT
button click.

Requires Visual Studio Build Tools with the "Desktop development with C++"
workload (provides `cl.exe`, CMake, Ninja, and vcpkg — already present on
this machine at `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools`).

```bat
sim\build.bat
sim\build\c5_lvgl_ui_sim.exe
```

`sim/build.bat` calls `vcvars64.bat` then invokes `cmake.exe`/`ninja.exe` by
full path rather than prepending them to `PATH` — a chained one-liner
(`call vcvars64.bat && set "PATH=...;%PATH%" && cmake ...`) looks fine but
silently breaks, because `%PATH%` in that `set` is substituted at cmd.exe's
*parse* time (before `vcvars64.bat` has actually run), so it captures the
*pre*-vcvars `PATH` and overwrites the good one — `cl.exe` then can't be
found even though `where cl` right after `vcvars64.bat` alone finds it
fine. Use the script rather than re-deriving that one-liner.

The first configure fetches SDL2 via vcpkg (manifest mode, `sim/vcpkg.json`)
and compiles LVGL from the same `managed_components/lvgl__lvgl` the ESP-IDF
build already pulled in — the ESP-IDF build must have run at least once
first so that directory exists. After the first build, re-running
`cmake --build build` picks up changes to anything under `main/ui/` in
seconds.

## Build (real hardware)

The toolchain was installed to `C:\Espressif\esp-idf` (ESP-IDF v5.5.5, tools
for both `esp32c5` and `esp32s3` targets). Open a new terminal and run:

```powershell
C:\Espressif\esp-idf\export.ps1
idf.py set-target esp32s3
idf.py build
idf.py -p <COM_PORT> flash monitor
```

The first `idf.py build` fetches LVGL and the other managed components listed
in `main/idf_component.yml` into `managed_components/`.

## Real-hardware readiness

- **Flashing/debugging** — OpenOCD and Xtensa GDB were installed by
  `install.ps1 esp32s3`; `idf.py flash`, `idf.py monitor`, and
  `idf.py openocd`/`idf.py gdb` work with no further setup. The board's
  USB-C port is native USB Serial/JTAG — Windows 10/11 enumerates it with
  its built-in driver, no separate USB driver install needed.
- **VS Code** — the official `espressif.esp-idf-extension` is installed
  (Command Palette → "ESP-IDF: ..."; point it at `C:\Espressif\esp-idf`).
- **OTA-ready partition table** — `CONFIG_PARTITION_TABLE_TWO_OTA_LARGE=y`
  (two 1700K app slots, no factory slot) so a later `esp_https_ota` update
  flow won't require re-partitioning devices already in the field. Current
  binary is ~1.19 MB, 30% free in each slot.
- **Branding/QR assets** — `CONFIG_LV_USE_LODEPNG` and `CONFIG_LV_USE_QRCODE`
  are enabled in both `sdkconfig.defaults` and `config/lv_conf_sim.h`, so
  PNG images (company logo) and `lv_qrcode` widgets (gift-redemption code)
  work identically on both targets. Convert source images with LVGL's
  bundled converter:
  ```
  C:\Users\ayush\.espressif\python_env\idf5.5_py3.10_env\Scripts\python.exe managed_components\lvgl__lvgl\scripts\LVGLImage.py --help
  ```
  (`pypng`/`lz4` already installed into that venv).
- **Onboarding captive portal** — done, see "Onboarding" above (hand-rolled
  on `esp_http_server` + SoftAP rather than the heavier
  `wifi_provisioning`/`protocomm` components, which weren't needed for a
  one-field-ish form).
- **Planned features already covered by ESP-IDF, no install needed** — BLE
  (in-tree `bt`/NimBLE), OTA (`esp_https_ota`), MQTT/HTTP client+server,
  actually connecting in STA mode using the WiFi credentials onboarding
  already saves to NVS (currently saved but unused). Just need `REQUIRES`
  added to `main/CMakeLists.txt` and sdkconfig options turned on when built.
- **microSD wiring documented, not yet used** — `config/board_config.h` has
  the 4-bit SDMMC pins; no driver code wired up yet (not needed while
  assets are embedded in the generous 16 MB flash / 8 MB PSRAM).

## Extending the UI

Write new faces/screens against `board_hal.h` only (no esp_lcd/FreeRTOS/SDL
calls) so they keep building for both targets. Anything touching LVGL
objects from outside the display's own update loop (e.g. a
`board_run_async()` job) must be wrapped in `board_lock()` / `board_unlock()`.
If a new feature needs a capability `board_hal.h` doesn't expose yet, add it
there and implement it in both `main/board_esp32.c` and `sim/board_sim.c`.
