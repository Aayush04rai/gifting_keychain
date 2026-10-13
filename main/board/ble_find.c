#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "host/ble_hs.h"
#include "host/ble_hs_pvcy.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "ble_find.h"
#include "board_hal.h"
#include "character.h"
#include "haptic_pattern.h"

void ble_store_config_init(void);

static const char *TAG = "ble_find";

// Test builds can turn the security requirements off so a laptop can exercise
// the ring path without a pairing dialog. MUST be 0 in anything that ships.
#ifndef FIND_INSECURE_TEST
#define FIND_INSECURE_TEST 0
#endif

// Test builds can print the passkey to the log (a laptop test reads it back).
// MUST be 0 in anything that ships: the code is meant to be seen only on the screen.
#ifndef FIND_LOG_PASSKEY
#define FIND_LOG_PASSKEY 0
#endif

#define PAIR_WINDOW_MS   (10u * 60 * 1000)
#define ALERT_MAX_MS     60000u
#define ALERT_GRACE_MS   2500u
#define CHECK_TICK_MS    500 // how often a ring checks whether it should stop
#define ADV_MIN_MS       1000
#define ADV_MAX_MS       1500

static uint8_t s_own_addr_type;
static volatile bool s_alerting;
static int64_t s_alert_t0_us;
static esp_timer_handle_t s_pulse_timer;
static uint8_t s_alert_level;

// --- Alerting (the "ring") -----------------------------------------------------

static void alert_stop(void)
{
    if (!s_alerting) {
        return;
    }
    s_alerting = false;
    esp_timer_stop(s_pulse_timer);
    haptic_pattern_stop();
    if (board_lock(500)) {
        character_find_stop();
        board_unlock();
    }
    ESP_LOGI(TAG, "ring stopped");
}

// Runs every CHECK_TICK_MS while ringing: ends the ring after ALERT_MAX_MS, or
// when the screen side has finished (the keychain was picked up). The buzzing
// itself is a haptic pattern that plays on its own timer.
static void pulse_cb(void *arg)
{
    int64_t elapsed_ms = (esp_timer_get_time() - s_alert_t0_us) / 1000;
    if (!s_alerting) {
        return;
    }
    if (elapsed_ms > ALERT_MAX_MS || (elapsed_ms > ALERT_GRACE_MS && !character_find_active())) {
        alert_stop();
    }
}

// Alert level 1 ("mild"): the gentle ring, an unobtrusive double tap. Level 2
// ("high", what find my keychain asks for): the strongest ring - it builds to
// continuous firm buzzes within a few seconds so it is felt through a bag.
// A ring ignores the vibration setting: it exists to be found.
static haptic_pattern_id_t pattern_for_level(uint8_t level)
{
    return level >= 2 ? HAPTIC_PAT_STRONGEST : HAPTIC_PAT_DOUBLE_TAP;
}

static void alert_start(uint8_t level)
{
    s_alert_t0_us = esp_timer_get_time();
    if (s_alerting) {
        // Asked again: restart the clock, and change pattern if the level changed.
        if (level != s_alert_level) {
            s_alert_level = level;
            haptic_pattern_start(pattern_for_level(level));
        }
        return;
    }
    s_alerting = true;
    s_alert_level = level;
    esp_timer_start_periodic(s_pulse_timer, CHECK_TICK_MS * 1000);
    haptic_pattern_start(pattern_for_level(level));
    if (board_lock(500)) {
        character_find_start();
        board_unlock();
    }
    ESP_LOGI(TAG, "ring started (level %u, pattern %s)", (unsigned)level, haptic_pattern_name(pattern_for_level(level)));
}

// --- GATT -----------------------------------------------------------------------

// Characteristics need an encrypted, authenticated (passkey-paired) link.
#if FIND_INSECURE_TEST
#define SEC_READ  0
#define SEC_WRITE 0
#else
#define SEC_READ  (BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_READ_AUTHEN)
#define SEC_WRITE (BLE_GATT_CHR_F_WRITE_ENC | BLE_GATT_CHR_F_WRITE_AUTHEN)
#endif

static int ias_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    uint8_t level = 0;
    if (OS_MBUF_PKTLEN(ctxt->om) < 1 || os_mbuf_copydata(ctxt->om, 0, 1, &level) != 0) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (level == 0) {
        alert_stop();
    } else {
        alert_start(level);
    }
    return 0;
}

static int battery_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int pct = board_get_battery_pct();
    uint8_t level = pct < 0 ? 100 : (uint8_t)pct; // no battery fitted / unknown: report full
    return os_mbuf_append(ctxt->om, &level, 1) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int info_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const char *text = (const char *)arg;
    return os_mbuf_append(ctxt->om, text, strlen(text)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static const struct ble_gatt_svc_def k_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x1802), // Immediate Alert
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(0x2A06), // Alert Level
                .access_cb = ias_access,
                .flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE | SEC_WRITE,
            },
            {0},
        },
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x180F), // Battery
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(0x2A19), // Battery Level
                .access_cb = battery_access,
                .flags = BLE_GATT_CHR_F_READ | SEC_READ,
            },
            {0},
        },
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x180A), // Device Information
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(0x2A24), // Model Number
                .access_cb = info_access,
                .arg = "Smart Keychain",
                .flags = BLE_GATT_CHR_F_READ | SEC_READ,
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2A26), // Firmware Revision
                .access_cb = info_access,
                .arg = "1.0",
                .flags = BLE_GATT_CHR_F_READ | SEC_READ,
            },
            {0},
        },
    },
    {0},
};

// --- GAP ------------------------------------------------------------------------

static void adv_start(void);

static bool pairing_open(void)
{
    if (esp_timer_get_time() / 1000 < (int64_t)PAIR_WINDOW_MS) {
        return true;
    }
    int bonds = 0;
    ble_store_util_count(BLE_STORE_OBJ_TYPE_OUR_SEC, &bonds);
    return bonds == 0; // never paired yet: stay open until it has been
}

static int gap_event(struct ble_gap_event *ev, void *arg)
{
    switch (ev->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connect %s (status %d)", ev->connect.status == 0 ? "ok" : "failed", ev->connect.status);
        if (ev->connect.status != 0) {
            adv_start();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected (reason %d)", ev->disconnect.reason);
        if (board_lock(300)) {
            character_pairing_done(); // a pairing card, if any, is no longer needed
            board_unlock();
        }
        adv_start();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        adv_start();
        break;
    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "link encryption changed (status %d)", ev->enc_change.status);
        if (ev->enc_change.status == 0 && board_lock(300)) {
            character_pairing_done();
            board_unlock();
        }
        break;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
        if (ev->passkey.params.action == BLE_SM_IOACT_DISP) {
            if (!pairing_open()) {
                ESP_LOGW(TAG, "pairing attempt outside the pairing window - refused");
                ble_gap_terminate(ev->passkey.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                break;
            }
            struct ble_sm_io io = {.action = BLE_SM_IOACT_DISP, .passkey = esp_random() % 1000000u};
            ESP_LOGI(TAG, "pairing: showing passkey on screen");
#if FIND_LOG_PASSKEY
            ESP_LOGI(TAG, "TESTPASSKEY %06u", (unsigned)io.passkey);
#endif
            if (board_lock(500)) {
                character_show_passkey(io.passkey);
                board_unlock();
            }
            ble_sm_inject_io(ev->passkey.conn_handle, &io);
        }
        break;
    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        // The phone lost its record of us (or is a new phone reusing an address):
        // forget the old bond and let it pair afresh - but only in the window.
        if (!pairing_open()) {
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        }
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(ev->repeat_pairing.conn_handle, &desc) == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }
    default:
        break;
    }
    return 0;
}

static void adv_start(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    static ble_uuid16_t uuids[] = {BLE_UUID16_INIT(0x1802)};
    fields.uuids16 = uuids;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
    const char *name = ble_svc_gap_device_name();
    fields.name = (const uint8_t *)name;
    fields.name_len = (uint8_t)strlen(name);
    fields.name_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields failed (%d)", rc);
        return;
    }
    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min = BLE_GAP_ADV_ITVL_MS(ADV_MIN_MS);
    params.itvl_max = BLE_GAP_ADV_ITVL_MS(ADV_MAX_MS);
    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "adv_start failed (%d)", rc);
    }
}

static void on_sync(void)
{
    // Private (rotating) addresses, so the keychain cannot be tracked by its address.
#if MYNEWT_VAL(BLE_HOST_BASED_PRIVACY)
    ble_hs_pvcy_rpa_config(NIMBLE_HOST_ENABLE_RPA);
#endif
    // With controller-based privacy (the ESP32-S3 default) asking for a private
    // address is enough: the controller rotates it.
    int rc = ble_hs_id_infer_auto(1, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "no usable address (%d)", rc);
        return;
    }
    adv_start();
    ESP_LOGI(TAG, "advertising as \"%s\" (free internal heap %u bytes)", ble_svc_gap_device_name(),
             (unsigned)esp_get_free_internal_heap_size());
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "host reset (reason %d)", reason);
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_find_start(void)
{
    const esp_timer_create_args_t pulse_args = {.callback = pulse_cb, .name = "find_pulse"};
    esp_timer_create(&pulse_args, &s_pulse_timer);

    if (nimble_port_init() != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth controller failed to start - find my keychain disabled");
        return;
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY; // shows a passkey, has no way to type one
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    int rc = ble_gatts_count_cfg(k_services);
    if (rc == 0) {
        rc = ble_gatts_add_svcs(k_services);
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT setup failed (%d)", rc);
        return;
    }

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    char name[24];
    snprintf(name, sizeof(name), "Keychain %02X%02X", mac[4], mac[5]);
    ble_svc_gap_device_name_set(name);

    ble_store_config_init(); // keeps pairings in NVS across reboots
    nimble_port_freertos_init(host_task);
}
