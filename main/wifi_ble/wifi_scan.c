#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "wifi_scan.h"

static const char *TAG = "wifi_scan";

static int compare_rssi_desc(const void *a, const void *b)
{
    const wifi_ap_record_t *ap_a = a;
    const wifi_ap_record_t *ap_b = b;
    return ap_b->rssi - ap_a->rssi;
}

esp_err_t wifi_scan_run(wifi_ap_record_t *out_records, uint16_t *out_count, uint16_t max_records)
{
    *out_count = 0;

    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set_mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi_start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active = {
            .min = 100,
            .max = 300,
        },
    };

    ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "scan_start failed: %s", esp_err_to_name(ret));
        esp_wifi_stop();
        return ret;
    }

    uint16_t ap_count = 0;
    ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret == ESP_OK && ap_count > 0) {
        wifi_ap_record_t records[WIFI_SCAN_MAX_APS];
        uint16_t record_count = ap_count > WIFI_SCAN_MAX_APS ? WIFI_SCAN_MAX_APS : ap_count;
        ret = esp_wifi_scan_get_ap_records(&record_count, records);
        if (ret == ESP_OK) {
            qsort(records, record_count, sizeof(records[0]), compare_rssi_desc);
            uint16_t copy_count = record_count > max_records ? max_records : record_count;
            memcpy(out_records, records, copy_count * sizeof(records[0]));
            *out_count = copy_count;
        }
    }

    esp_wifi_stop();
    return ret;
}
