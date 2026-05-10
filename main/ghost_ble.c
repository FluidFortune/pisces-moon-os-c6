// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_ble.c — BLE advertising scanner
//
//  Port of T-Deck wardrive.cpp BLE collection logic
//  to ESP-IDF C on RISC-V (ESP32-C6).
//
//  Uses NimBLE stack (lighter than Bluedroid).
//  Scans for BLE advertising packets passively.
//  Extracts MAC, name, RSSI, manufacturer data, addr type.
//  Emits ble_seen JSON to P4 via ghost_bridge.
// ============================================================

#include "ghost_ble.h"
#include "ghost_bridge.h"
#include "ghost_gps.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char* TAG = "GHOST_BLE";

int ghost_ble_devices = 0;

// ── GAP event handler ─────────────────────────────────────
static int _ble_gap_event(struct ble_gap_event* event, void* arg) {
    if (event->type != BLE_GAP_EVENT_DISC) return 0;
    if (!ghost_ble_active) return 0;

    struct ble_gap_disc_desc* desc = &event->disc;

    // MAC address
    char mac_str[18];
    uint8_t* a = desc->addr.val;
    snprintf(mac_str, sizeof(mac_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             a[5],a[4],a[3],a[2],a[1],a[0]);

    // Address type
    const char* addr_type = (desc->addr.type == BLE_ADDR_PUBLIC)
                            ? "public" : "random";

    // Device name from advertising data
    char name[32] = "";
    struct ble_hs_adv_fields fields;
    if (ble_hs_adv_parse_fields(&fields,
                                  desc->data,
                                  desc->length_data) == 0) {
        if (fields.name && fields.name_len > 0) {
            int len = fields.name_len < 31 ? fields.name_len : 31;
            memcpy(name, fields.name, len);
            name[len] = 0;
        }
    }

    // Manufacturer data as hex (first 8 bytes)
    char mfg_hex[32] = "";
    if (fields.mfg_data && fields.mfg_data_len > 0) {
        int len = fields.mfg_data_len < 8
                  ? fields.mfg_data_len : 8;
        for (int k = 0; k < len; k++) {
            snprintf(mfg_hex + k*2, sizeof(mfg_hex) - k*2,
                     "%02x", fields.mfg_data[k]);
        }
    }

    double lat = ghost_gps_lat();
    double lng = ghost_gps_lng();

    ghost_emit_ble_seen(mac_str, name, desc->rssi,
                         lat, lng, addr_type, mfg_hex);
    ghost_ble_devices++;

    return 0;
}

// ── NimBLE host task ─────────────────────────────────────
static void _nimble_host_task(void* pvArgs) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// ── Init ─────────────────────────────────────────────────
void ghost_ble_init(void) {
    nimble_port_init();
    nimble_port_freertos_init(_nimble_host_task);
    ESP_LOGI(TAG, "BLE scanner ready");
}

// ── BLE scan task ─────────────────────────────────────────
void ghost_ble_task(void* pvArgs) {
    ESP_LOGI(TAG, "BLE scan task running");

    struct ble_gap_disc_params disc_params = {
        .itvl             = 0,     // Use default
        .window           = 0,     // Use default
        .filter_policy    = BLE_HCI_SCAN_FILT_NO_WL,
        .limited          = 0,
        .passive          = 1,     // Passive scan — no scan requests
        .filter_duplicates = 0,    // Show all advertisements
    };

    while (1) {
        if (!ghost_ble_active) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Start 4-second BLE scan window (same as T-Deck)
        ble_gap_disc(BLE_OWN_ADDR_PUBLIC,
                     4000,          // Duration ms
                     &disc_params,
                     _ble_gap_event,
                     NULL);

        // Wait for scan to complete
        vTaskDelay(pdMS_TO_TICKS(4500));
    }
}
