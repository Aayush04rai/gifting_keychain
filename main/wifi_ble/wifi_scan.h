#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_wifi.h"

#define WIFI_SCAN_MAX_APS 10

// Blocking: brings STA mode up, scans, collects up to max_records APs sorted
// by RSSI (strongest first), then tears WiFi back down. Call from a task,
// never from the LVGL/BSP display task, since a scan can take a few seconds.
esp_err_t wifi_scan_run(wifi_ap_record_t *out_records, uint16_t *out_count, uint16_t max_records);
