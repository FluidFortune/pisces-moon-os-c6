// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_ble_gatt.c — GATT client (skeleton)
//
//  Built on the NimBLE/ESP-Bluedroid central API. The connect
//  flow is:
//    1. Stop scanner (Ghost wardrive BLE scan).
//    2. nimble_gap_connect(target).
//    3. On connect → service discovery → for each, char discovery.
//    4. Emit ble_service / ble_char events.
//    5. ble_read(handle) → gattc_read_by_handle.
//    6. On notification or read complete → ble_value event.
//
//  This file lays out the public API; the NimBLE plumbing is
//  conventional but bulky and is left as ENGINE_TODO.
// ============================================================

#include "ghost_ble_gatt.h"
#include "ghost_bridge.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char* TAG = "GHOST_GATT";

extern void ghost_emit_raw(const char* s);

// Forward
static void _emit_service(const char* uuid, int hs, int he);
static void _emit_char   (const char* uuid, int handle, const char* props);
static void _emit_value  (int handle, const uint8_t* data, int len);
static void _emit_connected(const char* mac);
static void _emit_disconnected(void);

void ghost_ble_gatt_init(void) {
    (void)_emit_service; (void)_emit_char;
    (void)_emit_value; (void)_emit_connected; (void)_emit_disconnected;
    ESP_LOGI(TAG, "[stub] GATT client init pending");
}

void ghost_ble_gatt_connect(const char* mac) {
    ESP_LOGI(TAG, "[stub] connect %s", mac ? mac : "(null)");
    // TODO: NimBLE central connect. On success → _emit_connected,
    //       trigger discovery → _emit_service / _emit_char.
}

void ghost_ble_gatt_disconnect(void) {
    ESP_LOGI(TAG, "[stub] disconnect");
}

void ghost_ble_gatt_read(int handle) {
    ESP_LOGI(TAG, "[stub] read h=%d", handle);
    // TODO: ble_gattc_read(conn_handle, handle, callback)
    //       in the callback → _emit_value(handle, data, len)
}

void ghost_ble_gatt_write(int handle, const char* hex) {
    ESP_LOGI(TAG, "[stub] write h=%d hex=%s", handle, hex ? hex : "(null)");
    // TODO: hex → bytes; ble_gattc_write(conn_handle, handle, bytes)
}

// ─── Emit helpers ────────────────────────────────────────────
static void _emit_service(const char* uuid, int hs, int he) {
    char buf[160];
    snprintf(buf, sizeof(buf),
              "{\"event\":\"ble_service\",\"uuid\":\"%s\","
              "\"handle_start\":%d,\"handle_end\":%d}\n",
              uuid ? uuid : "", hs, he);
    ghost_emit_raw(buf);
}

static void _emit_char(const char* uuid, int handle, const char* props) {
    char buf[200];
    snprintf(buf, sizeof(buf),
              "{\"event\":\"ble_char\",\"uuid\":\"%s\",\"handle\":%d,"
              "\"props\":\"%s\"}\n", uuid ? uuid : "", handle, props ? props : "");
    ghost_emit_raw(buf);
}

static void _emit_value(int handle, const uint8_t* data, int len) {
    char hexbuf[256];
    int n = (len > 120) ? 120 : len;
    for (int i = 0; i < n; i++) snprintf(hexbuf + i * 2, 3, "%02x", data[i]);
    hexbuf[n * 2] = 0;
    char buf[300];
    snprintf(buf, sizeof(buf),
              "{\"event\":\"ble_value\",\"handle\":%d,\"hex\":\"%s\"}\n",
              handle, hexbuf);
    ghost_emit_raw(buf);
}

static void _emit_connected(const char* mac) {
    char buf[80];
    snprintf(buf, sizeof(buf),
              "{\"event\":\"ble_connected\",\"mac\":\"%s\"}\n", mac ? mac : "");
    ghost_emit_raw(buf);
}

static void _emit_disconnected(void) {
    ghost_emit_raw("{\"event\":\"ble_disconnected\"}\n");
}
