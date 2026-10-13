// LVGL's memory: a private 4 MB heap carved out of the 8 MB PSRAM.
//
// Why a dedicated pool rather than the system heap or LVGL's built-in 64 KB
// one: the UI needs memory in bursts (the rounded eyes render through ~20 KB
// temporary layers; images from the SD card are cached; cards and acts add
// objects) and a fixed 64 KB ran out mid-frame, which in LVGL means a
// corrupted draw queue and a display task that spins forever. A private pool
// means the UI can never be starved by Wi-Fi/TLS/JSON buffers sharing the
// system heap, and can never starve them either; the internal RAM stays free
// for the radio.
//
// Selected with CONFIG_LV_USE_CUSTOM_MALLOC (sdkconfig.defaults). LVGL calls
// the functions below instead of its own allocator.

#include <stdlib.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "multi_heap.h"

#include "lvgl.h"

static const char *TAG = "lvgl_mem";

// How much PSRAM to give the UI. Tried in order until one can be reserved as
// a single contiguous block (it is requested at start-up, when PSRAM is nearly
// empty, so the first try succeeds in practice).
static const size_t k_pool_sizes[] = {4u * 1024 * 1024, 2u * 1024 * 1024, 1u * 1024 * 1024};

static multi_heap_handle_t s_heap;
static void *s_block;
static size_t s_pool_size;
static SemaphoreHandle_t s_lock;

static void lock(void)
{
    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }
}

static void unlock(void)
{
    if (s_lock) {
        xSemaphoreGive(s_lock);
    }
}

void lv_mem_init(void)
{
    if (s_heap) {
        return;
    }
    s_lock = xSemaphoreCreateMutex();

    for (size_t i = 0; i < sizeof(k_pool_sizes) / sizeof(k_pool_sizes[0]); i++) {
        s_block = heap_caps_malloc(k_pool_sizes[i], MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_block) {
            s_pool_size = k_pool_sizes[i];
            break;
        }
    }
    if (!s_block) {
        ESP_LOGE(TAG, "could not reserve any PSRAM for the UI pool");
        abort();
    }
    s_heap = multi_heap_register(s_block, s_pool_size);
    if (!s_heap) {
        ESP_LOGE(TAG, "multi_heap_register failed");
        abort();
    }
    ESP_LOGI(TAG, "UI pool: %u KB reserved in PSRAM", (unsigned)(s_pool_size / 1024));
}

void lv_mem_deinit(void)
{
    // The UI lives for the life of the device; nothing to tear down.
}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes)
{
    (void)mem;
    (void)bytes;
    return NULL; // a single fixed pool; growing it is not supported
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
    (void)pool;
}

void *lv_malloc_core(size_t size)
{
    lock();
    void *p = multi_heap_malloc(s_heap, size);
    unlock();
    return p;
}

void *lv_realloc_core(void *p, size_t new_size)
{
    lock();
    void *q = multi_heap_realloc(s_heap, p, new_size);
    unlock();
    return q;
}

void lv_free_core(void *p)
{
    if (!p) {
        return;
    }
    lock();
    multi_heap_free(s_heap, p);
    unlock();
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
    multi_heap_info_t info;
    lock();
    multi_heap_get_info(s_heap, &info);
    unlock();

    mon_p->total_size = s_pool_size;
    mon_p->free_size = info.total_free_bytes;
    mon_p->free_biggest_size = info.largest_free_block;
    mon_p->used_cnt = info.allocated_blocks;
    mon_p->free_cnt = info.free_blocks;
    mon_p->max_used = s_pool_size - info.minimum_free_bytes;
    mon_p->used_pct = (uint8_t)(100u - (100u * info.total_free_bytes) / s_pool_size);
    // Fragmentation: how much of the free space is not in the largest block.
    mon_p->frag_pct = info.total_free_bytes
                          ? (uint8_t)(100u - (100u * info.largest_free_block) / info.total_free_bytes)
                          : 0;
}

lv_result_t lv_mem_test_core(void)
{
    lock();
    bool ok = multi_heap_check(s_heap, true);
    unlock();
    return ok ? LV_RESULT_OK : LV_RESULT_INVALID;
}
