#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_hal.h"
#include "jokes.h"
#include "wifi_sta.h"

static const char *TAG = "jokes";

// JokeAPI in its strictest mode: Programming jokes only (the one category
// that is reliably workplace-friendly), single-line format, and every
// sensitive flag blacklisted. We ask for a handful and still vet each one
// ourselves in jokes_offer_external(), because the API's own filtering is a
// helpful first pass, not a guarantee.
#define JOKES_URL                                                                          \
    "https://v2.jokeapi.dev/joke/Programming?type=single&amount=5&safe-mode"               \
    "&blacklistFlags=nsfw,religious,political,racist,sexist,explicit"

// A few fresh jokes every couple of hours is plenty: the device only shows
// 1-3 a day, and the list below it already has dozens.
#define JOKES_POLL_INTERVAL_MS (2 * 60 * 60 * 1000)
#define JOKES_FIRST_DELAY_MS   (45 * 1000)
#define JOKES_BUF_CAP          (8 * 1024)

typedef struct {
    char *buf;
    size_t len;
} http_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        http_ctx_t *ctx = evt->user_data;
        if (ctx->len + (size_t)evt->data_len < JOKES_BUF_CAP) {
            memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
            ctx->len += evt->data_len;
            ctx->buf[ctx->len] = '\0';
        }
    }
    return ESP_OK;
}

static void parse_and_offer(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGW(TAG, "JSON parse failed");
        return;
    }
    cJSON *error = cJSON_GetObjectItemCaseSensitive(root, "error");
    cJSON *jokes = cJSON_GetObjectItemCaseSensitive(root, "jokes");
    if (cJSON_IsTrue(error) || !cJSON_IsArray(jokes)) {
        ESP_LOGW(TAG, "API returned an error or no jokes[]");
        cJSON_Delete(root);
        return;
    }
    int offered = 0, accepted = 0;
    // The pool is read from the display task (joke_next), so add to it under
    // the same lock that task holds while it runs.
    if (!board_lock(2000)) {
        cJSON_Delete(root);
        return;
    }
    cJSON *j = NULL;
    cJSON_ArrayForEach(j, jokes) {
        cJSON *text = cJSON_GetObjectItemCaseSensitive(j, "joke");
        if (cJSON_IsString(text) && text->valuestring) {
            offered++;
            if (jokes_offer_external(text->valuestring)) {
                accepted++;
            }
        }
    }
    board_unlock();
    ESP_LOGI(TAG, "%d of %d jokes passed the filter", accepted, offered);
    cJSON_Delete(root);
}

static void poll_once(void)
{
    char *buf = heap_caps_malloc(JOKES_BUF_CAP, MALLOC_CAP_SPIRAM);
    if (!buf) {
        return;
    }
    buf[0] = '\0';
    http_ctx_t ctx = {.buf = buf, .len = 0};

    esp_http_client_config_t cfg = {
        .url = JOKES_URL,
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
        parse_and_offer(buf);
    } else {
        ESP_LOGW(TAG, "fetch failed: err=%s status=%d (curated list still in use)",
                 esp_err_to_name(err), status);
    }
    free(buf);
}

static void jokes_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(JOKES_FIRST_DELAY_MS));
    while (1) {
        if (wifi_sta_is_connected()) {
            poll_once();
        }
        vTaskDelay(pdMS_TO_TICKS(JOKES_POLL_INTERVAL_MS));
    }
}

void jokes_start(void)
{
    xTaskCreate(jokes_task, "jokes", 5120, NULL, tskIDLE_PRIORITY + 1, NULL);
}
