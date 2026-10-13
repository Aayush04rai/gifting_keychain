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

#include "news.h"
#include "news_topics.h"
#include "onboarding.h"
#include "text_util.h"
#include "wifi_sta.h"

#include "../../config/api_keys.h"

static const char *TAG = "news";

#define GNEWS_BASE "https://gnews.io/api/v4/"
#define GNEWS_TAIL "&lang=en&max=3&apikey=" GNEWS_API_KEY

// One request per interval, whatever the number of topics, so the free
// tier's 100/day is never exceeded: 86400 s / 1080 s = 80 requests/day.
#define NEWS_POLL_INTERVAL_MS (18 * 60 * 1000)
// After the API says "too many requests" or "key rejected", leave it alone.
#define NEWS_BACKOFF_MS       (60 * 60 * 1000)
#define NEWS_FIRST_DELAY_MS   (20 * 1000)
#define NEWS_BUF_CAP          (32 * 1024)

#define NEWS_RING 16

static SemaphoreHandle_t s_lock;
static news_item_t s_ring[NEWS_RING];
static uint32_t s_hash[NEWS_RING];
static bool s_used[NEWS_RING];
static bool s_seen[NEWS_RING];
static uint32_t s_age[NEWS_RING]; // insertion order, for "oldest unseen first"
static uint32_t s_counter;
static size_t s_topic_idx;

typedef struct {
    char *buf;
    size_t len;
} http_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        http_ctx_t *ctx = evt->user_data;
        if (ctx->len + (size_t)evt->data_len < NEWS_BUF_CAP) {
            memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
            ctx->len += evt->data_len;
            ctx->buf[ctx->len] = '\0';
        }
    }
    return ESP_OK;
}

static uint32_t fnv1a(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) {
        h = (h ^ (uint8_t)*s) * 16777619u;
    }
    return h;
}

// "2026-10-07T08:30:00Z" -> UTC epoch seconds (0 if it does not parse). Done by
// hand with the days-from-civil algorithm because timegm() is not portable.
static int64_t parse_iso8601_utc(const char *s)
{
    int y, mo, d, h, mi, sec;
    if (!s || sscanf(s, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) != 6) {
        return 0;
    }
    y -= mo <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int64_t days = (int64_t)era * 146097 + doe - 719468;
    return days * 86400 + h * 3600 + mi * 60 + sec;
}
// Adds a headline unless one with the same text is already in the ring.
// Replaces an already-shown entry first, otherwise the oldest.
static void add_item(const news_topic_t *topic, const char *title, const char *source, int64_t published)
{
    news_item_t item = {.interest = topic->bit, .published = published};
    snprintf(item.category, sizeof(item.category), "%s", topic->label);
    snprintf(item.title, sizeof(item.title), "%s", title);
    text_to_ascii(item.title);
    if (source) {
        snprintf(item.source, sizeof(item.source), "%s", source);
        text_to_ascii(item.source);
    }
    if (strlen(item.title) < 12 || text_is_unsafe(item.title)) {
        return; // too short to mean anything, or not something to put on a gift
    }
    uint32_t h = fnv1a(item.title);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    int slot = -1;
    for (int i = 0; i < NEWS_RING; i++) {
        if (s_used[i] && s_hash[i] == h) {
            xSemaphoreGive(s_lock);
            return; // already have it
        }
    }
    for (int i = 0; i < NEWS_RING && slot < 0; i++) {
        if (!s_used[i]) {
            slot = i;
        }
    }
    if (slot < 0) {
        for (int i = 0; i < NEWS_RING; i++) {
            if (s_seen[i] && (slot < 0 || s_age[i] < s_age[slot])) {
                slot = i;
            }
        }
    }
    if (slot < 0) {
        slot = 0;
        for (int i = 1; i < NEWS_RING; i++) {
            if (s_age[i] < s_age[slot]) {
                slot = i;
            }
        }
    }
    s_ring[slot] = item;
    s_hash[slot] = h;
    s_used[slot] = true;
    s_seen[slot] = false;
    s_age[slot] = ++s_counter;
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG, "new [%s] %s", item.category, item.title);
}

static void parse_articles(const news_topic_t *topic, const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGW(TAG, "JSON parse failed");
        return;
    }
    cJSON *articles = cJSON_GetObjectItemCaseSensitive(root, "articles");
    if (cJSON_IsArray(articles)) {
        cJSON *a = NULL;
        cJSON_ArrayForEach(a, articles) {
            cJSON *title = cJSON_GetObjectItemCaseSensitive(a, "title");
            if (cJSON_IsString(title) && title->valuestring) {
                cJSON *when = cJSON_GetObjectItemCaseSensitive(a, "publishedAt");
                cJSON *src = cJSON_GetObjectItemCaseSensitive(a, "source");
                cJSON *src_name = src ? cJSON_GetObjectItemCaseSensitive(src, "name") : NULL;
                add_item(topic, title->valuestring,
                         cJSON_IsString(src_name) ? src_name->valuestring : NULL,
                         cJSON_IsString(when) ? parse_iso8601_utc(when->valuestring) : 0);
            }
        }
    } else {
        ESP_LOGW(TAG, "unexpected response (no articles[])");
    }
    cJSON_Delete(root);
}

// Returns false if the service told us to back off.
static bool poll_topic(const news_topic_t *topic)
{
    char url[256];
    snprintf(url, sizeof(url), GNEWS_BASE "%s" GNEWS_TAIL, topic->query);

    char *buf = heap_caps_malloc(NEWS_BUF_CAP, MALLOC_CAP_SPIRAM);
    if (!buf) {
        ESP_LOGE(TAG, "no memory for HTTP buffer");
        return true;
    }
    buf[0] = '\0';
    http_ctx_t ctx = {.buf = buf, .len = 0};

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 12000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    bool ok = true;
    if (err == ESP_OK && status == 200 && ctx.len > 0) {
        parse_articles(topic, buf);
    } else if (status == 401 || status == 403 || status == 429) {
        ESP_LOGW(TAG, "GNews refused (HTTP %d) - key invalid or quota used, backing off 1 h", status);
        ok = false;
    } else {
        ESP_LOGW(TAG, "fetch failed: err=%s status=%d", esp_err_to_name(err), status);
    }
    free(buf);
    return ok;
}

static void news_task(void *arg)
{
    (void)arg;
    uint16_t interests = onboarding_get_interests();

    // The topics this recipient actually picked, in table order.
    const news_topic_t *mine[NEWS_TOPIC_COUNT];
    size_t n = 0;
    for (size_t i = 0; i < NEWS_TOPIC_COUNT; i++) {
        if (interests & NEWS_TOPICS[i].bit) {
            mine[n++] = &NEWS_TOPICS[i];
        }
    }
    ESP_LOGI(TAG, "%u topic(s) selected, polling one every %d min", (unsigned)n,
             NEWS_POLL_INTERVAL_MS / 60000);

    vTaskDelay(pdMS_TO_TICKS(NEWS_FIRST_DELAY_MS));
    while (1) {
        uint32_t wait = NEWS_POLL_INTERVAL_MS;
        if (wifi_sta_is_connected()) {
            if (!poll_topic(mine[s_topic_idx++ % n])) {
                wait = NEWS_BACKOFF_MS;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}

// Topics that have a news source, as a mask.
static uint16_t news_topic_mask(void)
{
    uint16_t mask = 0;
    for (size_t i = 0; i < NEWS_TOPIC_COUNT; i++) {
        mask |= NEWS_TOPICS[i].bit;
    }
    return mask;
}

bool news_available(void)
{
    return s_lock != NULL;
}

void news_start(void)
{
    if (!(onboarding_get_interests() & news_topic_mask())) {
        return;
    }
    if (GNEWS_API_KEY[0] == '\0') {
        ESP_LOGW(TAG, "GNEWS_API_KEY is empty (config/api_keys.h) - news reactions are off");
        return;
    }
    s_lock = xSemaphoreCreateMutex();
    xTaskCreate(news_task, "news", 6144, NULL, tskIDLE_PRIORITY + 1, NULL);
}

bool news_take(news_item_t *out)
{
    if (!s_lock) {
        return false;
    }
    bool ok = false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int best = -1;
    for (int i = 0; i < NEWS_RING; i++) {
        if (s_used[i] && !s_seen[i] && (best < 0 || s_age[i] < s_age[best])) {
            best = i;
        }
    }
    if (best >= 0) {
        *out = s_ring[best];
        s_seen[best] = true;
        ok = true;
    }
    xSemaphoreGive(s_lock);
    return ok;
}
