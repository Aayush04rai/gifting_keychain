#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include "lvgl.h"

#include "board_hal.h"
#include "onboarding.h"

static const char *TAG = "onboarding";

// Recipient connects their phone to this network to reach the setup page.
// WPA2 rather than open: several phones refuse to list or join open APs that
// have no internet. The password is on the setup screen and in the QR code.
#define ONBOARD_AP_SSID "ThinkRobotics Setup"
#define ONBOARD_AP_PASS "gift2026"

#define NVS_NAMESPACE    "onboard"
#define NVS_KEY_DONE     "done"
#define NVS_KEY_NAME     "name"
#define NVS_KEY_TITLE    "title"
// "interests2": 16-bit mask. The original "interests" key held 8 bits; a new
// name avoids an NVS type mismatch on devices set up with the old firmware
// (they simply read as "no interests" until set up again).
#define NVS_KEY_INTERESTS "interests2"
#define NVS_KEY_COMPANION "companion"
#define NVS_KEY_TZ        "tz"
#define NVS_KEY_VIBRATION "vib"
#define TZ_MAX_LEN        40
#define NVS_KEY_SSID     "wifi_ssid"
#define NVS_KEY_PASS     "wifi_pass"

#define NAME_MAX_LEN  32
#define TITLE_MAX_LEN 32
#define SSID_MAX_LEN  32
#define PASS_MAX_LEN  64

// Form embeds these directly from main/onboarding/form.html and thanks.html
// (see main/CMakeLists.txt's EMBED_TXTFILES) rather than as C string
// literals, so the actual markup/CSS is easy to read and edit.
extern const uint8_t form_html_start[] asm("_binary_form_html_start");
extern const uint8_t form_html_end[] asm("_binary_form_html_end");
extern const uint8_t thanks_html_start[] asm("_binary_thanks_html_start");
extern const uint8_t thanks_html_end[] asm("_binary_thanks_html_end");

static char s_name[NAME_MAX_LEN] = "there";
static char s_title[TITLE_MAX_LEN];
static uint16_t s_interests;
static char s_wifi_ssid[SSID_MAX_LEN];
static char s_wifi_pass[PASS_MAX_LEN];
static companion_t s_companion = COMPANION_MALE;
static char s_tz[TZ_MAX_LEN] = "IST-5:30";
static bool s_vibration = true;
static volatile bool s_form_submitted;

// --- NVS ---

static bool nvs_load_onboarding(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    uint8_t done = 0;
    esp_err_t ret = nvs_get_u8(handle, NVS_KEY_DONE, &done);
    if (ret == ESP_OK && done) {
        size_t len = sizeof(s_name);
        nvs_get_str(handle, NVS_KEY_NAME, s_name, &len);
        len = sizeof(s_title);
        nvs_get_str(handle, NVS_KEY_TITLE, s_title, &len);
        nvs_get_u16(handle, NVS_KEY_INTERESTS, &s_interests);
        uint8_t companion = COMPANION_MALE; // absent on devices set up before the question existed
        if (nvs_get_u8(handle, NVS_KEY_COMPANION, &companion) == ESP_OK) {
            s_companion = companion == COMPANION_FEMALE ? COMPANION_FEMALE : COMPANION_MALE;
        }
        uint8_t vib = 1; // absent on devices set up before the question existed: on
        if (nvs_get_u8(handle, NVS_KEY_VIBRATION, &vib) == ESP_OK) {
            s_vibration = vib != 0;
        }
        len = sizeof(s_tz);
        nvs_get_str(handle, NVS_KEY_TZ, s_tz, &len); // absent on older devices: keeps the default
        len = sizeof(s_wifi_ssid);
        nvs_get_str(handle, NVS_KEY_SSID, s_wifi_ssid, &len);
        len = sizeof(s_wifi_pass);
        nvs_get_str(handle, NVS_KEY_PASS, s_wifi_pass, &len);
    }
    nvs_close(handle);

#ifdef ONBOARD_FORCE_FEMALE
    // One-off: set the companion without redoing the setup form. The choice is
    // stored, so the flag can be removed again after a single boot.
    if (ret == ESP_OK && done && s_companion != COMPANION_FEMALE) {
        s_companion = COMPANION_FEMALE;
        nvs_handle_t rw;
        if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &rw) == ESP_OK) {
            nvs_set_u8(rw, NVS_KEY_COMPANION, (uint8_t)COMPANION_FEMALE);
            nvs_commit(rw);
            nvs_close(rw);
            ESP_LOGI(TAG, "companion set to female");
        }
    }
#endif
    return ret == ESP_OK && done;
}

static void nvs_save_onboarding(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed, setup will re-run next boot");
        return;
    }
    nvs_set_str(handle, NVS_KEY_NAME, s_name);
    nvs_set_str(handle, NVS_KEY_TITLE, s_title);
    nvs_set_u16(handle, NVS_KEY_INTERESTS, s_interests);
    nvs_set_u8(handle, NVS_KEY_COMPANION, (uint8_t)s_companion);
    nvs_set_u8(handle, NVS_KEY_VIBRATION, s_vibration ? 1 : 0);
    nvs_set_str(handle, NVS_KEY_TZ, s_tz);
    nvs_set_str(handle, NVS_KEY_SSID, s_wifi_ssid);
    nvs_set_str(handle, NVS_KEY_PASS, s_wifi_pass);
    nvs_set_u8(handle, NVS_KEY_DONE, 1);
    nvs_commit(handle);
    nvs_close(handle);
}

// --- HTTP form ---

// Decodes application/x-www-form-urlencoded in place: '+' -> space,
// '%XX' -> byte. httpd_query_key_value() (used to pull fields out of the
// raw body) does not do this itself.
static void url_decode(char *s)
{
    char *out = s;
    while (*s) {
        if (*s == '+') {
            *out++ = ' ';
            s++;
        } else if (*s == '%' && s[1] && s[2]) {
            char hex[3] = {s[1], s[2], 0};
            *out++ = (char)strtol(hex, NULL, 16);
            s += 3;
        } else {
            *out++ = *s++;
        }
    }
    *out = '\0';
}

// Checkboxes only appear in the POST body at all when checked ("on"), so
// presence of the key - not its value - is what matters here.
static bool checkbox_checked(const char *body, const char *key)
{
    char scratch[8];
    return httpd_query_key_value(body, key, scratch, sizeof(scratch)) == ESP_OK;
}

typedef struct {
    const char *field;
    uint16_t bit;
} interest_field_t;

// Keep in sync with the checkbox `name`s in form.html.
static const interest_field_t INTEREST_FIELDS[] = {
    {"int_cricket", ONBOARD_INTEREST_CRICKET},
    {"int_f1", ONBOARD_INTEREST_F1},
    {"int_football", ONBOARD_INTEREST_FOOTBALL},
    {"int_tech", ONBOARD_INTEREST_TECH},
    {"int_finance", ONBOARD_INTEREST_FINANCE},
    {"int_travel", ONBOARD_INTEREST_TRAVEL},
    {"int_ai", ONBOARD_INTEREST_AI},
    {"int_robotics", ONBOARD_INTEREST_ROBOTICS},
    {"int_semi", ONBOARD_INTEREST_SEMI},
    {"int_space", ONBOARD_INTEREST_SPACE},
    {"int_ev", ONBOARD_INTEREST_EV},
    {"int_health", ONBOARD_INTEREST_HEALTH},
    {"int_defence", ONBOARD_INTEREST_DEFENCE},
    {"int_cyber", ONBOARD_INTEREST_CYBER},
};

static esp_err_t root_get_handler(httpd_req_t *req)
{
    return httpd_resp_send(req, (const char *)form_html_start, form_html_end - form_html_start);
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    // 14 checkboxes + name/title + a 63-char WiFi password that can grow 3x
    // when URL-encoded: comfortably under 768, not safely under 512.
    char body[1024];
    size_t len = req->content_len < sizeof(body) - 1 ? req->content_len : sizeof(body) - 1;
    int received = httpd_req_recv(req, body, len);
    if (received <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    body[received] = '\0';

    // Each scratch buffer is sized to exactly match its destination so
    // snprintf can never truncate (httpd_query_key_value() already caps
    // output at the buffer size it's given).
    char name_value[NAME_MAX_LEN];
    if (httpd_query_key_value(body, "name", name_value, sizeof(name_value)) == ESP_OK) {
        url_decode(name_value);
        snprintf(s_name, sizeof(s_name), "%s", name_value);
    }
    char title_value[TITLE_MAX_LEN];
    if (httpd_query_key_value(body, "title", title_value, sizeof(title_value)) == ESP_OK) {
        url_decode(title_value);
        snprintf(s_title, sizeof(s_title), "%s", title_value);
    }
    char ssid_value[SSID_MAX_LEN];
    if (httpd_query_key_value(body, "wifi_ssid", ssid_value, sizeof(ssid_value)) == ESP_OK) {
        url_decode(ssid_value);
        snprintf(s_wifi_ssid, sizeof(s_wifi_ssid), "%s", ssid_value);
    }
    char pass_value[PASS_MAX_LEN];
    if (httpd_query_key_value(body, "wifi_password", pass_value, sizeof(pass_value)) == ESP_OK) {
        url_decode(pass_value);
        snprintf(s_wifi_pass, sizeof(s_wifi_pass), "%s", pass_value);
    }

    char companion_value[8]; // "male" / "female"
    if (httpd_query_key_value(body, "companion", companion_value, sizeof(companion_value)) == ESP_OK) {
        s_companion = strcmp(companion_value, "female") == 0 ? COMPANION_FEMALE : COMPANION_MALE;
    }

    char vib_value[8]; // "on" / "off"
    if (httpd_query_key_value(body, "vibration", vib_value, sizeof(vib_value)) == ESP_OK) {
        s_vibration = strcmp(vib_value, "off") != 0;
    }

    // The time zone arrives URL-encoded (commas and slashes become %xx), so
    // read it into a larger buffer, decode, and accept only the characters a
    // POSIX TZ string uses - never anything else into setenv().
    char tz_value[TZ_MAX_LEN * 3];
    if (httpd_query_key_value(body, "tz", tz_value, sizeof(tz_value)) == ESP_OK) {
        url_decode(tz_value);
        bool ok = tz_value[0] != '\0' && strlen(tz_value) < TZ_MAX_LEN;
        for (const char *c = tz_value; ok && *c; c++) {
            ok = (*c >= '0' && *c <= '9') || (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || *c == '+' ||
                 *c == '-' || *c == ':' || *c == ',' || *c == '.' || *c == '/';
        }
        if (ok) {
            snprintf(s_tz, sizeof(s_tz), "%s", tz_value);
        }
    }

    s_interests = 0;
    for (size_t i = 0; i < sizeof(INTEREST_FIELDS) / sizeof(INTEREST_FIELDS[0]); i++) {
        if (checkbox_checked(body, INTEREST_FIELDS[i].field)) {
            s_interests |= INTEREST_FIELDS[i].bit;
        }
    }

    nvs_save_onboarding();

    esp_err_t ret = httpd_resp_send(req, (const char *)thanks_html_start, thanks_html_end - thanks_html_start);
    s_form_submitted = true;
    return ret;
}

// --- On-device screens ---

static void show_setup_screen(void)
{
    if (!board_lock(0)) {
        return;
    }

    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *qr = lv_qrcode_create(scr);
    lv_qrcode_set_size(qr, 128);
    lv_qrcode_set_dark_color(qr, lv_color_hex(0x000000));
    lv_qrcode_set_light_color(qr, lv_color_hex(0xffffff));
    char qr_payload[96];
    snprintf(qr_payload, sizeof(qr_payload), "WIFI:T:WPA;S:%s;P:%s;;", ONBOARD_AP_SSID, ONBOARD_AP_PASS);
    lv_qrcode_set_data(qr, qr_payload);
    lv_obj_align(qr, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Scan to connect,");
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_align(title, LV_ALIGN_TOP_RIGHT, -8, 20);

    lv_obj_t *title2 = lv_label_create(scr);
    lv_label_set_text(title2, "then open:");
    lv_obj_set_style_text_color(title2, lv_color_hex(0xffffff), 0);
    lv_obj_align(title2, LV_ALIGN_TOP_RIGHT, -8, 44);

    lv_obj_t *url = lv_label_create(scr);
    lv_label_set_text(url, "192.168.4.1");
    lv_obj_set_style_text_color(url, lv_color_hex(0xd4af6a), 0);
    lv_obj_set_style_text_font(url, &lv_font_montserrat_20, 0);
    lv_obj_align(url, LV_ALIGN_TOP_RIGHT, -8, 72);

    lv_obj_t *ssid = lv_label_create(scr);
    lv_label_set_text(ssid, ONBOARD_AP_SSID);
    lv_obj_set_style_text_color(ssid, lv_color_hex(0x9fb3c8), 0);
    lv_obj_align(ssid, LV_ALIGN_BOTTOM_RIGHT, -8, -8);

    lv_obj_t *pass = lv_label_create(scr);
    lv_label_set_text(pass, "Password: " ONBOARD_AP_PASS);
    lv_obj_set_style_text_color(pass, lv_color_hex(0x9fb3c8), 0);
    lv_obj_align(pass, LV_ALIGN_BOTTOM_RIGHT, -8, -30);

    board_unlock();
}

static void show_welcome_screen(void)
{
    if (!board_lock(0)) {
        return;
    }

    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    char greeting[48];
    snprintf(greeting, sizeof(greeting), "Welcome, %s", s_name);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, greeting);
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_center(label);

    board_unlock();
}

// --- AP + server lifecycle ---

static void wifi_event_logger(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id) {
    case WIFI_EVENT_AP_START:
        ESP_LOGI(TAG, "AP started");
        break;
    case WIFI_EVENT_AP_STACONNECTED: {
        wifi_event_ap_staconnected_t *e = data;
        ESP_LOGI(TAG, "phone joined: " MACSTR, MAC2STR(e->mac));
        break;
    }
    case WIFI_EVENT_AP_STADISCONNECTED: {
        wifi_event_ap_stadisconnected_t *e = data;
        ESP_LOGI(TAG, "phone left: " MACSTR " reason=%d", MAC2STR(e->mac), e->reason);
        break;
    }
    case WIFI_EVENT_AP_PROBEREQRECVED:
        ESP_LOGD(TAG, "probe request");
        break;
    }
}

static httpd_handle_t start_setup_server(void)
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_logger, NULL));

    wifi_config_t ap_config = {
        .ap = {
            .channel = 6,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = 4,
        },
    };
    snprintf((char *)ap_config.ap.ssid, sizeof(ap_config.ap.ssid), "%s", ONBOARD_AP_SSID);
    snprintf((char *)ap_config.ap.password, sizeof(ap_config.ap.password), "%s", ONBOARD_AP_PASS);
    ap_config.ap.ssid_len = strlen(ONBOARD_AP_SSID);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "AP config: ssid='%s' ch=6 WPA2", ONBOARD_AP_SSID);

    httpd_handle_t server = NULL;
    httpd_config_t http_cfg = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&server, &http_cfg));

    static const httpd_uri_t root_uri = {
        .uri = "/", .method = HTTP_GET, .handler = root_get_handler,
    };
    static const httpd_uri_t save_uri = {
        .uri = "/save", .method = HTTP_POST, .handler = save_post_handler,
    };
    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &save_uri);

    return server;
}

static void stop_setup_server(httpd_handle_t server)
{
    // Give the "All set!" HTTP response time to actually reach the phone
    // before the AP that's carrying it disappears.
    vTaskDelay(pdMS_TO_TICKS(1500));

    httpd_stop(server);
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_logger);
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif) {
        esp_netif_destroy_default_wifi(ap_netif);
    }
}

void onboarding_run_if_needed(void)
{
    if (nvs_load_onboarding()) {
        return;
    }

    ESP_LOGI(TAG, "Not yet onboarded - starting setup AP '%s'", ONBOARD_AP_SSID);
    show_setup_screen();

    httpd_handle_t server = start_setup_server();

    s_form_submitted = false;
    while (!s_form_submitted) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    stop_setup_server(server);
    show_welcome_screen();
    vTaskDelay(pdMS_TO_TICKS(2000));
}

const char *onboarding_get_name(void)
{
    return s_name;
}

const char *onboarding_get_title(void)
{
    return s_title;
}

bool onboarding_get_vibration(void)
{
    return s_vibration;
}

const char *onboarding_get_tz(void)
{
    return s_tz;
}

companion_t onboarding_get_companion(void)
{
    return s_companion;
}

uint16_t onboarding_get_interests(void)
{
    return s_interests;
}

const char *onboarding_get_wifi_ssid(void)
{
    return s_wifi_ssid;
}

const char *onboarding_get_wifi_pass(void)
{
    return s_wifi_pass;
}
