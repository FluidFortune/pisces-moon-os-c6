// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_promisc.c — 802.11 monitor-mode bridge
//
//  Hot-path filter: skip frames the P4 doesn't care about
//  (per s_filter_mask) before formatting JSON. Format itself
//  goes via ghost_emit_pkt(). At full mgmt-only flow we'll
//  see ~50–200 frames/sec on a busy channel — UART at 921600
//  comfortably absorbs that.
// ============================================================

#include "ghost_promisc.h"
#include "ghost_bridge.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "GHOST_PROMISC";

static volatile bool   s_active      = false;
static uint8_t         s_filter_mask = 0xFF;

// 802.11 frame_ctrl helpers
#define FC_TYPE(fc)    (((fc) >> 2) & 0x3)
#define FC_SUBTYPE(fc) (((fc) >> 4) & 0xF)
#define FC_TYPE_MGMT 0
#define FC_TYPE_CTRL 1
#define FC_TYPE_DATA 2

static const char* _frame_type_str(uint8_t type) {
    switch (type) {
        case FC_TYPE_MGMT: return "mgmt";
        case FC_TYPE_CTRL: return "ctrl";
        case FC_TYPE_DATA: return "data";
        default:            return "rsvd";
    }
}

static bool _wanted(uint8_t type) {
    if (type == FC_TYPE_MGMT && (s_filter_mask & 1)) return true;
    if (type == FC_TYPE_DATA && (s_filter_mask & 2)) return true;
    if (type == FC_TYPE_CTRL && (s_filter_mask & 4)) return true;
    return false;
}

static void _mac_fmt(char* out, const uint8_t* m) {
    snprintf(out, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
              m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void _on_packet(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!s_active || !buf) return;
    wifi_promiscuous_pkt_t* p = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* payload = p->payload;
    if (p->rx_ctrl.sig_len < 24) return;
    uint16_t fc      = ((uint16_t)payload[0]) | ((uint16_t)payload[1] << 8);
    uint8_t  ftype   = FC_TYPE(fc);
    if (!_wanted(ftype)) return;

    char src[18], dst[18], bssid[18];
    _mac_fmt(dst,   payload + 4);
    _mac_fmt(src,   payload + 10);
    _mac_fmt(bssid, payload + 16);

    ghost_emit_pkt(_frame_type_str(ftype), src, dst, bssid, "",
                    p->rx_ctrl.channel, p->rx_ctrl.rssi);
}

void ghost_promisc_init(void) {
    esp_wifi_set_promiscuous_rx_cb(_on_packet);
    ESP_LOGI(TAG, "promisc cb registered");
}

void ghost_promisc_start(void) {
    s_active = true;
    esp_wifi_set_promiscuous(true);
    ESP_LOGI(TAG, "active mask=0x%02x", s_filter_mask);
}

void ghost_promisc_stop(void) {
    s_active = false;
    esp_wifi_set_promiscuous(false);
    ESP_LOGI(TAG, "stopped");
}

void ghost_promisc_set_filter(uint8_t mask) { s_filter_mask = mask; }

bool ghost_promisc_is_active(void) { return s_active; }
