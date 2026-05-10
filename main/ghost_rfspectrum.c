// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


#include "ghost_rfspectrum.h"
#include "ghost_bridge.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "GHOST_RFSPEC";

#define DWELL_MS    220     // total per-channel time
#define NUM_CHANS   14

static volatile bool s_running = false;
static int           s_packets[NUM_CHANS];
static int           s_rssi_sum[NUM_CHANS];
static int           s_rssi_count[NUM_CHANS];
static int           s_current_channel = 0;

extern void ghost_emit_raw(const char* s);

static void _on_pkt(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!s_running || !buf) return;
    wifi_promiscuous_pkt_t* p = (wifi_promiscuous_pkt_t*)buf;
    int ch = s_current_channel;
    if (ch < 0 || ch >= NUM_CHANS) return;
    s_packets[ch]++;
    s_rssi_sum[ch]   += p->rx_ctrl.rssi;
    s_rssi_count[ch] += 1;
}

static void _emit_channel(int ch) {
    int floor = (s_rssi_count[ch] > 0)
                 ? (s_rssi_sum[ch] / s_rssi_count[ch])
                 : -90;
    int util  = s_packets[ch];   // simplified: pkt count per dwell
    if (util > 100) util = 100;
    char buf[120];
    snprintf(buf, sizeof(buf),
              "{\"event\":\"channel\",\"ch\":%d,\"rssi_floor\":%d,"
              "\"util_pct\":%d}\n", ch + 1, floor, util);
    ghost_emit_raw(buf);
}

static void _task(void* arg) {
    (void)arg;
    esp_wifi_set_promiscuous_rx_cb(_on_pkt);
    esp_wifi_set_promiscuous(true);
    while (s_running) {
        for (int ch = 0; ch < NUM_CHANS && s_running; ch++) {
            s_current_channel  = ch;
            s_packets[ch]      = 0;
            s_rssi_sum[ch]     = 0;
            s_rssi_count[ch]   = 0;
            esp_wifi_set_channel(ch + 1, WIFI_SECOND_CHAN_NONE);
            vTaskDelay(pdMS_TO_TICKS(DWELL_MS));
            _emit_channel(ch);
        }
    }
    esp_wifi_set_promiscuous(false);
    vTaskDelete(NULL);
}

void ghost_rfspectrum_init(void) { /* nothing */ }

void ghost_rfspectrum_start(void) {
    if (s_running) return;
    s_running = true;
    xTaskCreate(_task, "ghost_rfspec", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "sweep started");
}

void ghost_rfspectrum_stop(void) { s_running = false; }
