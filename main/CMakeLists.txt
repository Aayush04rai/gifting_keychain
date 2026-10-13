idf_component_register(
    SRCS "main.c"
         "board/board_esp32.c" "board/qmi8658.c" "board/lvgl_mem_psram.c" "board/sd_upload.c" "board/ble_find.c" "board/haptic_pattern.c"
         "wifi_ble/wifi_scan.c" "wifi_ble/wifi_sta.c"
         "onboarding/onboarding_esp32.c"
         "personalization/news_esp32.c" "personalization/jokes_esp32.c"
         "personalization/jokes_core.c" "personalization/text_util.c" "personalization/wellness.c"
         "events/events.c" "character/card/card.c" "power/power_esp32.c"
         "character/character.c" "character/expressions.c" "character/acts/acts.c"
         "character/eyes/eyes.c" "character/mouth/mouth.c" "character/extras/extras.c" "character/status/status.c" "character/led/led.c"
         "ui/watchface_companion.c"
         "ui/ui.c" "ui/gesture.c"
    INCLUDE_DIRS "." "board" "wifi_ble" "onboarding" "personalization" "character" "events" "power" "ui"
    EMBED_TXTFILES "onboarding/form.html" "onboarding/thanks.html"
    REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_timer esp_http_server esp_pm esp_driver_usb_serial_jtag heap
             fatfs sdmmc esp_driver_sdmmc vfs bt
             esp_lcd esp_driver_spi esp_driver_i2c esp_driver_gpio esp_driver_ledc esp_adc
             esp_http_client esp-tls json
)

# LVGL's own library is linked after this one, so the linker would never go back for
# the allocator defined in board/lvgl_mem_psram.c (CONFIG_LV_USE_CUSTOM_MALLOC).
# Forcing the symbol in pulls the whole object file.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--undefined=lv_malloc_core")
