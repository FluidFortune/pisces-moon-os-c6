// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_wifi.c — WiFi scan + promiscuous mode
//
//  Port of T-Deck wardrive.cpp WiFi collection logic
//  to ESP-IDF C on RISC-V (ESP32-C6).
//
//  No display code. No SPI Treaty (radio not on same bus
//  as SD/LoRa — those are on the P4 side).
//  Pure collection → emit JSON to P4.
// ============================================================

#include "ghost_wifi.h"
#include "ghost_bridge.h"
#include "ghost_gps.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = "GHOST_WIFI";

int ghost_networks_found = 0;

// ── Encryption type string ────────────────────────────────
// Same as T-Deck _wifi_enc_str() — full detail for Clinician
static const char* _enc_str(wifi_auth_mode_t enc) {
    switch (enc) {
        case WIFI_AUTH_OPEN:          return "OPEN";
        case WIFI_AUTH_WEP:           return "WEP";
        case WIFI_AUTH_WPA_PSK:       return "WPA";
        case WIFI_AUTH_WPA2_PSK:      return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:  return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK:      return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        default:                       return "UNKNOWN";
    }
}

// ── Init ─────────────────────────────────────────────────
void ghost_wifi_init(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_start();

    ESP_LOGI(TAG, "WiFi scanner ready (scan mode)");
}

// ── WiFi scan task ────────────────────────────────────────
void ghost_wifi_task(void* pvArgs) {
    wifi_scan_config_t scan_cfg = {
        .ssid        = NULL,
        .bssid       = NULL,
        .channel     = 0,      // Scan all channels
        .show_hidden = true,
        .scan_type   = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300,
    };

    ESP_LOGI(TAG, "WiFi scan task running");

    while (1) {
        if (!ghost_wardrive_active) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Active scan
        esp_wifi_scan_start(&scan_cfg, true); // blocking

        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);

        if (ap_count > 0) {
            wifi_ap_record_t* records = malloc(
                ap_count * sizeof(wifi_ap_record_t));
            if (records) {
                esp_wifi_scan_get_ap_records(&ap_count, records);

                for (int i = 0; i < ap_count; i++) {
                    char mac_str[18];
                    uint8_t* b = records[i].bssid;
                    snprintf(mac_str, sizeof(mac_str),
                             "%02X:%02X:%02X:%02X:%02X:%02X",
                             b[0],b[1],b[2],b[3],b[4],b[5]);

                    double lat = ghost_gps_lat();
                    double lng = ghost_gps_lng();

                    ghost_emit_wifi_seen(
                        mac_str,
                        (const char*)records[i].ssid,
                        records[i].rssi,
                        records[i].primary,
                        _enc_str(records[i].authmode),
                        lat, lng
                    );

                    ghost_networks_found++;
                }
                free(records);
            }
        }

        // Scan cycle every 8 seconds (same as T-Deck)
        vTaskDelay(pdMS_TO_TICKS(8000));
    }
}

// ─────────────────────────────────────────────
//  STA control (Phase 11 — bridge dispatcher API)
//
//  Three flags drive the wifi task:
//    s_scan_pending    — forces a fresh scan on next cycle
//    s_assoc_target    — non-empty: try to associate to this SSID
//    s_assoc_pass      — pass for that SSID
//
//  When association succeeds we emit a wifi_connected event;
//  when it drops, wifi_disconnected. esp_event_handler hooks
//  on WIFI_EVENT and IP_EVENT do the actual emission.
// ─────────────────────────────────────────────
#include "esp_event.h"
extern void ghost_emit_raw(const char* s);

static volatile bool s_scan_pending     = false;
static volatile bool s_disconnect_pending = false;
static char          s_assoc_target[33] = "";
static char          s_assoc_pass[64]   = "";

void ghost_wifi_scan_request(void)        { s_scan_pending = true; }
void ghost_wifi_disconnect_request(void)  { s_disconnect_pending = true; }

void ghost_wifi_connect(const char* ssid, const char* pass) {
    if (!ssid || !ssid[0]) return;
    strncpy(s_assoc_target, ssid, sizeof(s_assoc_target) - 1);
    s_assoc_target[sizeof(s_assoc_target) - 1] = 0;
    if (pass) {
        strncpy(s_assoc_pass, pass, sizeof(s_assoc_pass) - 1);
        s_assoc_pass[sizeof(s_assoc_pass) - 1] = 0;
    } else {
        s_assoc_pass[0] = 0;
    }

    wifi_config_t cfg = {0};
    strncpy((char*)cfg.sta.ssid,     s_assoc_target, sizeof(cfg.sta.ssid)     - 1);
    strncpy((char*)cfg.sta.password, s_assoc_pass,   sizeof(cfg.sta.password) - 1);
    cfg.sta.threshold.authmode = s_assoc_pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();
}

void ghost_wifi_emit_status(void) {
    wifi_ap_record_t ap; bool connected = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK);
    char buf[256];
    if (connected) {
        snprintf(buf, sizeof(buf),
                  "{\"event\":\"wifi_connected\",\"ssid\":\"%s\",\"rssi\":%d,\"ch\":%d}\n",
                  (char*)ap.ssid, ap.rssi, ap.primary);
    } else {
        snprintf(buf, sizeof(buf), "{\"event\":\"wifi_disconnected\"}\n");
    }
    ghost_emit_raw(buf);
}
