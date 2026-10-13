#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "cricket.h"
#include "onboarding.h"
#include "wifi_sta.h"

#include "../../config/api_keys.h"

static const char *TAG = "cricket";

#define CRICKET_URL "https://api.cricapi.com/v1/currentMatches?apikey=" CRICKET_API_KEY "&offset=0"

// CricketData.org's free tier is 100 requests/day; 15 minutes keeps us at
// ~96/day with margin, and cricket_task() below only polls at all when the
// recipient actually checked "cricket" during onboarding.
#define CRICKET_POLL_INTERVAL_MS (15 * 60 * 1000)
#define CRICKET_BUF_CAP (32 * 1024)

static SemaphoreHandle_t s_lock;
static cricket_score_t s_latest;

typedef struct {
    char *buf;
    size_t len;
} http_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        http_ctx_t *ctx = evt->user_data;
        if (ctx->len + (size_t)evt->data_len < CRICKET_BUF_CAP) {
            memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
            ctx->len += evt->data_len;
            ctx->buf[ctx->len] = '\0';
        }
    }
    return ESP_OK;
}

static void copy_json_string(cJSON *obj, const char *key, char *dst, size_t dst_size)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring) {
        snprintf(dst, dst_size, "%s", item->valuestring);
    }
}

// The exact shape of each "score[]" entry (runs/wickets/overs per innings)
// isn't pinned down in CricketData.org's public docs at the time this was
// written, so this is deliberately defensive: any missing field just
// leaves that piece blank instead of failing the whole parse - the match
// status text (always present) is enough on its own to be useful.
static void format_innings(cJSON *score_item, char *score_out, size_t score_size, char *overs_out,
                            size_t overs_size)
{
    if (!score_item) {
        return;
    }
    cJSON *r = cJSON_GetObjectItemCaseSensitive(score_item, "r");
    cJSON *w = cJSON_GetObjectItemCaseSensitive(score_item, "w");
    cJSON *o = cJSON_GetObjectItemCaseSensitive(score_item, "o");
    if (cJSON_IsNumber(r)) {
        snprintf(score_out, score_size, "%d-%d", r->valueint, cJSON_IsNumber(w) ? w->valueint : 0);
    }
    if (cJSON_IsNumber(o)) {
        snprintf(overs_out, overs_size, "%.1f ov", o->valuedouble);
    }
}

static void parse_and_store(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGW(TAG, "JSON parse failed");
        return;
    }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsArray(data)) {
        cJSON *status = cJSON_GetObjectItemCaseSensitive(root, "status");
        ESP_LOGW(TAG, "unexpected response shape (no data[]) - status: %s",
                 cJSON_IsString(status) ? status->valuestring : "?");
        cJSON_Delete(root);
        return;
    }

    // Prefer an actually-live match; fall back to the first entry (likely
    // just-finished or about-to-start) so the face always shows something.
    cJSON *chosen = NULL;
    cJSON *match = NULL;
    cJSON_ArrayForEach(match, data) {
        cJSON *started = cJSON_GetObjectItemCaseSensitive(match, "matchStarted");
        cJSON *ended = cJSON_GetObjectItemCaseSensitive(match, "matchEnded");
        if (cJSON_IsTrue(started) && cJSON_IsFalse(ended)) {
            chosen = match;
            break;
        }
    }
    if (!chosen) {
        chosen = cJSON_GetArrayItem(data, 0);
    }
    if (!chosen) {
        cJSON_Delete(root);
        return;
    }

    cricket_score_t fresh = {0};
    cJSON *teams = cJSON_GetObjectItemCaseSensitive(chosen, "teams");
    if (cJSON_IsArray(teams) && cJSON_GetArraySize(teams) >= 2) {
        cJSON *t1 = cJSON_GetArrayItem(teams, 0);
        cJSON *t2 = cJSON_GetArrayItem(teams, 1);
        if (cJSON_IsString(t1)) {
            snprintf(fresh.team1, sizeof(fresh.team1), "%s", t1->valuestring);
        }
        if (cJSON_IsString(t2)) {
            snprintf(fresh.team2, sizeof(fresh.team2), "%s", t2->valuestring);
        }
    }
    copy_json_string(chosen, "status", fresh.status, sizeof(fresh.status));

    cJSON *score = cJSON_GetObjectItemCaseSensitive(chosen, "score");
    if (cJSON_IsArray(score)) {
        format_innings(cJSON_GetArrayItem(score, 0), fresh.score1, sizeof(fresh.score1), fresh.overs1,
                        sizeof(fresh.overs1));
        format_innings(cJSON_GetArrayItem(score, 1), fresh.score2, sizeof(fresh.score2), fresh.overs2,
                        sizeof(fresh.overs2));
    }
    fresh.valid = true;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_latest = fresh;
    xSemaphoreGive(s_lock);

    cJSON_Delete(root);
}

static void poll_once(void)
{
    char *buf = heap_caps_malloc(CRICKET_BUF_CAP, MALLOC_CAP_SPIRAM);
    if (!buf) {
        ESP_LOGE(TAG, "no memory for HTTP buffer");
        return;
    }
    buf[0] = '\0';
    http_ctx_t ctx = {.buf = buf, .len = 0};

    esp_http_client_config_t cfg = {
        .url = CRICKET_URL,
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err == ESP_OK && status == 200 && ctx.len > 0) {
        parse_and_store(buf);
    } else {
        ESP_LOGW(TAG, "fetch failed: err=%s status=%d", esp_err_to_name(err), status);
    }
    free(buf);
}

static void cricket_task(void *arg)
{
    (void)arg;
    while (1) {
        if (wifi_sta_is_connected()) {
            poll_once();
        }
        vTaskDelay(pdMS_TO_TICKS(CRICKET_POLL_INTERVAL_MS));
    }
}

void cricket_start(void)
{
    if (!(onboarding_get_interests() & ONBOARD_INTEREST_CRICKET)) {
        return;
    }
    s_lock = xSemaphoreCreateMutex();
    xTaskCreate(cricket_task, "cricket", 6144, NULL, tskIDLE_PRIORITY + 1, NULL);
}

bool cricket_get(cricket_score_t *out)
{
    if (!s_lock) {
        return false;
    }
    bool ok = false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_latest.valid) {
        *out = s_latest;
        ok = true;
    }
    xSemaphoreGive(s_lock);
    return ok;
}
