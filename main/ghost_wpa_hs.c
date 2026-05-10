// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_wpa_hs.c — Handshake collector (skeleton)
//
//  Pairs the typical M1/M2 from station-AP exchanges. Two
//  TODOs remaining:
//    1. Per-AP nonce buffer keyed on (BSSID, STA-MAC).
//    2. hccapx assembly per Hashcat's binary format.
//
//  When a complete pair is observed we'd call
//  ghost_emit_handshake(ssid, bssid, hccapx_b64).
// ============================================================

#include "ghost_wpa_hs.h"
#include "ghost_bridge.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "mbedtls/base64.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char* TAG = "GHOST_WPA";

static volatile bool s_running = false;

extern void ghost_emit_raw(const char* s);

// Forward
static void _emit_handshake(const char* ssid, const char* bssid,
                              const uint8_t* hccapx, int len);

static const uint8_t LLC_EAPOL[8] = { 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E };

static void _on_pkt(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!s_running || !buf) return;
    wifi_promiscuous_pkt_t* p = (wifi_promiscuous_pkt_t*)buf;
    if (p->rx_ctrl.sig_len < 32) return;
    const uint8_t* d = p->payload;
    uint16_t fc = ((uint16_t)d[0]) | ((uint16_t)d[1] << 8);
    uint8_t  ftype = (fc >> 2) & 0x3;
    if (ftype != 2) return;     // data frames only

    // Check for LLC SNAP EAPOL at body offset (24 for 3-addr, 30 if QoS).
    int body = 24;
    if ((fc >> 4) & 0x8) body = 26;     // QoS subfield bumps header by 2
    if (p->rx_ctrl.sig_len < body + 8) return;
    if (memcmp(d + body, LLC_EAPOL, 8) != 0) return;

    // We've seen an EAPOL frame. TODO:
    //   - Extract (BSSID, STA-MAC) from addr1/addr2 (depends on FromDS/ToDS).
    //   - Track msg-num via EAPOL Key Information bits (offset body+9..).
    //   - On {M1, M2} pair → assemble hccapx → _emit_handshake.

    // For now: log a sighting so the P4 sees something is happening.
    char hbuf[160];
    snprintf(hbuf, sizeof(hbuf),
              "{\"event\":\"eapol_seen\",\"rssi\":%d,\"ch\":%d}\n",
              p->rx_ctrl.rssi, p->rx_ctrl.channel);
    ghost_emit_raw(hbuf);
}

static void _emit_handshake(const char* ssid, const char* bssid,
                              const uint8_t* hccapx, int len) {
    if (!hccapx || len <= 0) return;
    size_t need = ((len + 2) / 3) * 4 + 1;
    char* b64 = (char*)malloc(need + 32);
    if (!b64) return;
    size_t got = 0;
    if (mbedtls_base64_encode((unsigned char*)b64, need, &got,
                               hccapx, len) != 0) {
        free(b64);
        return;
    }
    b64[got] = 0;
    char hdr[120];
    snprintf(hdr, sizeof(hdr),
              "{\"event\":\"handshake\",\"ssid\":\"%s\",\"bssid\":\"%s\",\"hccapx_b64\":\"",
              ssid ? ssid : "", bssid ? bssid : "");
    ghost_emit_raw(hdr);
    ghost_emit_raw(b64);
    ghost_emit_raw("\"}\n");
    free(b64);
}

void ghost_wpa_hs_init(void) { (void)_emit_handshake; }

void ghost_wpa_hs_start(void) {
    if (s_running) return;
    s_running = true;
    esp_wifi_set_promiscuous_rx_cb(_on_pkt);
    esp_wifi_set_promiscuous(true);
    ESP_LOGI(TAG, "collector started");
}

void ghost_wpa_hs_stop(void) {
    s_running = false;
    esp_wifi_set_promiscuous(false);
    ESP_LOGI(TAG, "collector stopped");
}
