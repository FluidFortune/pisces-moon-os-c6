// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_bridge.c — C6 → P4 JSON Bridge protocol
//
//  Implements the same JSON event protocol as the T-Deck
//  bridge_app.cpp, over UART instead of USB Serial.
//
//  The P4 treats this stream identically to a T-Deck in
//  Bridge mode. pm_bridge.py on the host side sees no
//  difference — the protocol is identical.
// ============================================================

#include "ghost_bridge.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "GHOST_BRIDGE";

// ── State flags ───────────────────────────────────────────
volatile bool ghost_wardrive_active    = true;   // Start wardriving at boot
volatile bool ghost_ble_active         = true;   // BLE active at boot
volatile bool ghost_promiscuous_active = false;  // Promiscuous off by default
volatile bool ghost_raw_log            = false;  // Raw log off by default
volatile bool ghost_bridge_streaming   = true;   // Always streaming to P4

// ── UART buffer ───────────────────────────────────────────
#define UART_BUF_SIZE 1024
#define EMIT_BUF_SIZE 512

static char _emit_buf[EMIT_BUF_SIZE];

// ── Init ─────────────────────────────────────────────────
void ghost_bridge_init(void) {
    uart_config_t cfg = {
        .baud_rate  = GHOST_BRIDGE_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_param_config(GHOST_BRIDGE_UART_NUM, &cfg);
    uart_set_pin(GHOST_BRIDGE_UART_NUM,
                 GHOST_BRIDGE_TX_PIN,
                 GHOST_BRIDGE_RX_PIN,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);
    uart_driver_install(GHOST_BRIDGE_UART_NUM,
                        UART_BUF_SIZE, UART_BUF_SIZE,
                        0, NULL, 0);

    ESP_LOGI(TAG, "Bridge UART init: %d baud TX=%d RX=%d",
             GHOST_BRIDGE_BAUD,
             GHOST_BRIDGE_TX_PIN,
             GHOST_BRIDGE_RX_PIN);
}

// ── Raw emit ─────────────────────────────────────────────
static void _emit(const char* json) {
    uart_write_bytes(GHOST_BRIDGE_UART_NUM,
                     json, strlen(json));
}

// Public — used by ghost_http, ghost_netscan, ghost_rfspectrum,
// ghost_wpa_hs, ghost_ble_gatt, ghost_wifi_ducky to send fragments
// of an event line. Each call writes raw bytes; callers compose
// the JSON themselves and end with "\n" on the last fragment.
void ghost_emit_raw(const char* s) {
    if (!s || !*s) return;
    uart_write_bytes(GHOST_BRIDGE_UART_NUM, s, strlen(s));
}

// ── Event emitters ────────────────────────────────────────

void ghost_emit_ready(void) {
    snprintf(_emit_buf, sizeof(_emit_buf),
        "{\"event\":\"ready\","
        "\"firmware\":\"%s\","
        "\"version\":\"%s\","
        "\"ghost_engine\":\"active\"}\n",
        GHOST_FIRMWARE_NAME, GHOST_VERSION);
    _emit(_emit_buf);
    ESP_LOGI(TAG, "Ready event sent to P4");
}

void ghost_emit_wifi_seen(const char* mac, const char* ssid,
                           int rssi, int ch, const char* enc,
                           double lat, double lng) {
    // Sanitize SSID — no quotes or backslashes
    char safe_ssid[36];
    strncpy(safe_ssid, ssid, 35);
    safe_ssid[35] = 0;
    for (int i = 0; safe_ssid[i]; i++) {
        if (safe_ssid[i] == '"' || safe_ssid[i] == '\\')
            safe_ssid[i] = '_';
    }

    snprintf(_emit_buf, sizeof(_emit_buf),
        "{\"event\":\"wifi_seen\","
        "\"mac\":\"%s\","
        "\"ssid\":\"%s\","
        "\"rssi\":%d,"
        "\"ch\":%d,"
        "\"enc\":\"%s\","
        "\"lat\":%.6f,"
        "\"lng\":%.6f}\n",
        mac, safe_ssid, rssi, ch, enc, lat, lng);
    _emit(_emit_buf);
}

void ghost_emit_ble_seen(const char* mac, const char* name,
                          int rssi, double lat, double lng,
                          const char* addr_type,
                          const char* mfg_hex) {
    char safe_name[36];
    strncpy(safe_name, name ? name : "", 35);
    safe_name[35] = 0;
    for (int i = 0; safe_name[i]; i++) {
        if (safe_name[i] == '"' || safe_name[i] == '\\')
            safe_name[i] = '_';
    }

    snprintf(_emit_buf, sizeof(_emit_buf),
        "{\"event\":\"ble_seen\","
        "\"mac\":\"%s\","
        "\"name\":\"%s\","
        "\"rssi\":%d,"
        "\"lat\":%.6f,"
        "\"lng\":%.6f,"
        "\"addr_type\":\"%s\","
        "\"mfg_data\":\"%s\"}\n",
        mac, safe_name, rssi, lat, lng,
        addr_type ? addr_type : "unknown",
        mfg_hex ? mfg_hex : "");
    _emit(_emit_buf);
}

void ghost_emit_probe_seen(const char* mac, const char* ssid,
                            int rssi, int count,
                            double lat, double lng) {
    char safe_ssid[36];
    strncpy(safe_ssid, ssid && ssid[0] ? ssid : "(wildcard)", 35);
    safe_ssid[35] = 0;
    for (int i = 0; safe_ssid[i]; i++) {
        if (safe_ssid[i] == '"' || safe_ssid[i] == '\\')
            safe_ssid[i] = '_';
    }

    snprintf(_emit_buf, sizeof(_emit_buf),
        "{\"event\":\"probe_seen\","
        "\"mac\":\"%s\","
        "\"ssid\":\"%s\","
        "\"rssi\":%d,"
        "\"count\":%d,"
        "\"lat\":%.6f,"
        "\"lng\":%.6f}\n",
        mac, safe_ssid, rssi, count, lat, lng);
    _emit(_emit_buf);
}

void ghost_emit_pkt(const char* frame_type, const char* src,
                     const char* dst, const char* bssid,
                     const char* ssid, int ch, int rssi) {
    snprintf(_emit_buf, sizeof(_emit_buf),
        "{\"event\":\"pkt\","
        "\"frame_type\":\"%s\","
        "\"src\":\"%s\","
        "\"dst\":\"%s\","
        "\"bssid\":\"%s\","
        "\"ssid\":\"%s\","
        "\"ch\":%d,"
        "\"rssi\":%d}\n",
        frame_type, src, dst, bssid,
        ssid ? ssid : "", ch, rssi);
    _emit(_emit_buf);
}

void ghost_emit_gps(double lat, double lng, double alt,
                     int sats, bool valid, double speed) {
    snprintf(_emit_buf, sizeof(_emit_buf),
        "{\"event\":\"gps\","
        "\"lat\":%.6f,"
        "\"lng\":%.6f,"
        "\"alt_m\":%.1f,"
        "\"sats\":%d,"
        "\"valid\":%s,"
        "\"speed\":%.2f}\n",
        lat, lng, alt, sats,
        valid ? "true" : "false", speed);
    _emit(_emit_buf);
}

void ghost_emit_status(void) {
    extern int ghost_networks_found;
    extern int ghost_ble_devices;

    snprintf(_emit_buf, sizeof(_emit_buf),
        "{\"event\":\"status\","
        "\"ghost_engine\":\"%s\","
        "\"wardrive_active\":%s,"
        "\"ble_active\":%s,"
        "\"promiscuous_active\":%s,"
        "\"raw_log\":%s}\n",
        ghost_wardrive_active ? "active" : "idle",
        ghost_wardrive_active ? "true" : "false",
        ghost_ble_active      ? "true" : "false",
        ghost_promiscuous_active ? "true" : "false",
        ghost_raw_log         ? "true" : "false");
    _emit(_emit_buf);
}

// ── Command listener task ─────────────────────────────────
//
// Receives JSON commands from P4 over UART. Each command is one
// JSON object terminated by '\n'. We parse with cJSON to get the
// "cmd" field and any args.
//
// Commands are routed to ghost_wifi / ghost_ble / ghost_promisc /
// ghost_http / ghost_hid / ghost_ble_gatt / ghost_netscan /
// ghost_rfspectrum / ghost_wpa_hs / ghost_wifi_ducky.

#include "cJSON.h"
#include "ghost_wifi.h"
#include "ghost_ble.h"
#include "ghost_promisc.h"
#include "ghost_http.h"
#include "ghost_hid.h"
#include "ghost_ble_gatt.h"
#include "ghost_netscan.h"
#include "ghost_rfspectrum.h"
#include "ghost_wpa_hs.h"
#include "ghost_wifi_ducky.h"

static const char* _str(cJSON* j, const char* key) {
    cJSON* v = cJSON_GetObjectItem(j, key);
    return (v && cJSON_IsString(v) && v->valuestring) ? v->valuestring : "";
}
static int _int(cJSON* j, const char* key) {
    cJSON* v = cJSON_GetObjectItem(j, key);
    return (v && cJSON_IsNumber(v)) ? (int)v->valuedouble : 0;
}

static void _dispatch(const char* line) {
    cJSON* root = cJSON_Parse(line);
    if (!root) {
        ESP_LOGD(TAG, "non-JSON: %s", line);
        return;
    }
    const char* cmd = _str(root, "cmd");
    if (!*cmd) { cJSON_Delete(root); return; }

    // ── Engine-state toggles ───────────────────────────
    if      (!strcmp(cmd, "wardrive_start")) ghost_wardrive_active = true;
    else if (!strcmp(cmd, "wardrive_stop"))  ghost_wardrive_active = false;
    else if (!strcmp(cmd, "ble_start"))      ghost_ble_active = true;
    else if (!strcmp(cmd, "ble_stop"))       ghost_ble_active = false;
    else if (!strcmp(cmd, "raw_log_start"))  ghost_raw_log = true;
    else if (!strcmp(cmd, "raw_log_stop"))   ghost_raw_log = false;
    else if (!strcmp(cmd, "ping"))           _emit("{\"event\":\"pong\","
                                                    "\"firmware\":\"GhostEngine-P4-C6\"}\n");
    else if (!strcmp(cmd, "status"))         ghost_emit_status();

    // ── Wifi STA control ───────────────────────────────
    else if (!strcmp(cmd, "wifi_scan"))       ghost_wifi_scan_request();
    else if (!strcmp(cmd, "wifi_connect"))    ghost_wifi_connect(_str(root, "ssid"),
                                                                  _str(root, "pass"));
    else if (!strcmp(cmd, "wifi_disconnect")) ghost_wifi_disconnect_request();
    else if (!strcmp(cmd, "wifi_status"))     ghost_wifi_emit_status();

    // ── Promiscuous capture ───────────────────────────
    else if (!strcmp(cmd, "promiscuous_start"))  ghost_promisc_start();
    else if (!strcmp(cmd, "promiscuous_stop"))   ghost_promisc_stop();
    else if (!strcmp(cmd, "promiscuous_filter")) ghost_promisc_set_filter(
                                                       (uint8_t)_int(root, "mask"));

    // ── HTTP proxy ─────────────────────────────────────
    else if (!strcmp(cmd, "http_get"))  ghost_http_get((uint32_t)_int(root, "id"),
                                                         _str(root, "url"),
                                                         _str(root, "bearer"));
    else if (!strcmp(cmd, "http_post")) {
        const char* body = _str(root, "body");
        ghost_http_post((uint32_t)_int(root, "id"),
                         _str(root, "url"), _str(root, "bearer"),
                         _str(root, "ct"), body, body ? (int)strlen(body) : 0);
    }

    // ── BLE HID ────────────────────────────────────────
    else if (!strcmp(cmd, "hid_pair"))       ghost_hid_pair_start();
    else if (!strcmp(cmd, "hid_disconnect")) ghost_hid_pair_stop();
    else if (!strcmp(cmd, "hid_string"))     ghost_hid_send_string(_str(root, "text"));
    else if (!strcmp(cmd, "hid_key"))        ghost_hid_send_key   (_str(root, "key"));

    // ── BLE GATT client ───────────────────────────────
    else if (!strcmp(cmd, "ble_connect"))    ghost_ble_gatt_connect(_str(root, "mac"));
    else if (!strcmp(cmd, "ble_disconnect")) ghost_ble_gatt_disconnect();
    else if (!strcmp(cmd, "ble_read"))       ghost_ble_gatt_read(_int(root, "handle"));
    else if (!strcmp(cmd, "ble_write"))      ghost_ble_gatt_write(_int(root, "handle"),
                                                                    _str(root, "hex"));

    // ── Network scan ──────────────────────────────────
    else if (!strcmp(cmd, "net_scan"))       ghost_netscan_start();

    // ── RF spectrum ───────────────────────────────────
    else if (!strcmp(cmd, "rf_spectrum_start")) ghost_rfspectrum_start();
    else if (!strcmp(cmd, "rf_spectrum_stop"))  ghost_rfspectrum_stop();

    // ── WPA handshake collector ───────────────────────
    else if (!strcmp(cmd, "wpa_hs_start"))   ghost_wpa_hs_start();
    else if (!strcmp(cmd, "wpa_hs_stop"))    ghost_wpa_hs_stop();

    // ── WiFi-Ducky AP ─────────────────────────────────
    else if (!strcmp(cmd, "wifi_ducky_ap_start"))
        ghost_wifi_ducky_ap_start(_str(root, "ssid"), _str(root, "pass"));
    else if (!strcmp(cmd, "wifi_ducky_ap_stop"))
        ghost_wifi_ducky_ap_stop();

    else ESP_LOGW(TAG, "unknown cmd '%s'", cmd);

    cJSON_Delete(root);
}

void ghost_cmd_task(void* pvArgs) {
    uint8_t  rxbuf[256];
    char     linebuf[1024];
    int      linelen = 0;

    ESP_LOGI(TAG, "Command listener running (cJSON dispatch)");

    while (1) {
        int len = uart_read_bytes(GHOST_BRIDGE_UART_NUM,
                                   rxbuf, sizeof(rxbuf) - 1,
                                   pdMS_TO_TICKS(10));
        if (len <= 0) continue;

        for (int i = 0; i < len; i++) {
            char c = (char)rxbuf[i];
            if (c == '\n' || linelen >= (int)sizeof(linebuf) - 2) {
                linebuf[linelen] = 0;
                if (linelen > 0) _dispatch(linebuf);
                linelen = 0;
            } else {
                linebuf[linelen++] = c;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
