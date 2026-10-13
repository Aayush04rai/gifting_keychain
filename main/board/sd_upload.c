#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "board_hal.h"

// Copies files onto the microSD over the USB cable, so the card never has to
// come out of the product. tools/sd_push.py is the host side. The protocol is
// one text line followed by the raw file bytes:
//
//   host:   SDPUT <path under the card root> <size>\n<size bytes>
//   device: SDOK <path>   or   SDERR <reason>
//
// Anything that is not an SDPUT line (stray bytes, a terminal) is ignored.

static const char *TAG = "sd_upload";

#define ROOT "/sdcard/"
// The host sends BLOCK bytes at a time and waits for an ACK byte: the USB
// driver drops bytes if its ring buffer fills while the card is busy writing.
#define BLOCK 1024
#define ACK 0x06

static int read_byte(TickType_t wait)
{
    uint8_t b;
    return usb_serial_jtag_read_bytes(&b, 1, wait) == 1 ? b : -1;
}

static void reply(const char *kind, const char *text)
{
    // Leading newline so the marker starts its own line amid log output.
    char buf[96];
    int n = snprintf(buf, sizeof(buf), "\n%s %s\n", kind, text);
    usb_serial_jtag_write_bytes(buf, n, pdMS_TO_TICKS(500));
}

static void make_parent_dirs(char *full)
{
    for (char *p = full + strlen(ROOT); *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(full, 0777);
            *p = '/';
        }
    }
}

static void receive_file(const char *path, long size)
{
    if (strstr(path, "..") || path[0] == '/' || strlen(path) > 60 || size < 0 || size > 8 * 1024 * 1024) {
        reply("SDERR", "bad path or size");
        return;
    }
    char full[80];
    snprintf(full, sizeof(full), ROOT "%s", path);
    make_parent_dirs(full);
    FILE *f = fopen(full, "wb");
    bool ok = f != NULL;

    // The bytes must be consumed even if the file could not be opened, or
    // they would be misread as commands.
    uint8_t buf[BLOCK];
    long left = size;
    while (left > 0) {
        int want = left > BLOCK ? BLOCK : (int)left;
        int got = 0;
        while (got < want) {
            int n = usb_serial_jtag_read_bytes(buf + got, want - got, pdMS_TO_TICKS(3000));
            if (n <= 0) {
                break;
            }
            got += n;
        }
        if (got < want) {
            ok = false; // the host went away mid-file
            break;
        }
        if (ok && fwrite(buf, 1, got, f) != (size_t)got) {
            ok = false;
        }
        left -= got;
        uint8_t ack = ok ? ACK : 0x15; // NAK: the card refused the write
        usb_serial_jtag_write_bytes(&ack, 1, pdMS_TO_TICKS(500));
    }
    if (f) {
        fclose(f);
    }
    if (ok) {
        ESP_LOGI(TAG, "wrote %s (%ld bytes)", path, size);
        reply("SDOK", path);
    } else {
        char why[48];
        snprintf(why, sizeof(why), left > 0 ? "interrupted, %ld left" : "cannot write", left);
        reply("SDERR", why);
    }
}

static void upload_task(void *arg)
{
    char line[96];
    int len = 0;
    for (;;) {
        int c = read_byte(pdMS_TO_TICKS(1000));
        if (c < 0) {
            continue;
        }
        if (c != '\n') {
            if (len < (int)sizeof(line) - 1) {
                line[len++] = (char)c;
            }
            continue;
        }
        line[len] = 0;
        len = 0;
        char path[64];
        long size;
        if (sscanf(line, "SDPUT %63s %ld", path, &size) == 2) {
            receive_file(path, size);
        }
    }
}

void sd_upload_start(void)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.rx_buffer_size = 4096;
    cfg.tx_buffer_size = 512;
    if (usb_serial_jtag_driver_install(&cfg) != ESP_OK) {
        ESP_LOGW(TAG, "USB driver unavailable - SD upload disabled");
        return;
    }
    xTaskCreate(upload_task, "sd_upload", 4096, NULL, 2, NULL);
}
