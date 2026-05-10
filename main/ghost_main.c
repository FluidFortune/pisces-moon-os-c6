// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_main.c — C6 Ghost Engine entry point
//
//  This is the firmware that replaces AT command firmware on
//  the ESP32-C6-MINI-1 onboard the CrowPanel Advanced 7inch.
//
//  Flash via:
//    1. SDIO OTA from P4 (preferred, no soldering)
//    2. esptool.py via UART1 header (fallback)
//
//  The Ghost Engine never stops.
//  It wardrives from the moment the C6 has power.
//  The P4 can be in bootloader mode. Jennifer can be offline.
//  The C6 keeps collecting.
// ============================================================

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "ghost_bridge.h"
#include "ghost_wifi.h"
#include "ghost_ble.h"
#include "ghost_gps.h"
#include "ghost_promisc.h"
#include "ghost_http.h"
#include "ghost_hid.h"
#include "ghost_ble_gatt.h"
#include "ghost_netscan.h"
#include "ghost_rfspectrum.h"
#include "ghost_wpa_hs.h"
#include "ghost_wifi_ducky.h"

static const char* TAG = "GHOST_MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "=== Pisces Moon OS — Ghost Engine C6 v%s ===",
             GHOST_FIRMWARE_NAME);
    ESP_LOGI(TAG, "The Ghost Engine never stops.");

    // Initialize NVS (required by WiFi stack)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // ── Init Bridge UART (C6 → P4 JSON stream) ───────────
    ghost_bridge_init();

    // ── Emit ready event ──────────────────────────────────
    // P4 is waiting for this to know C6 Ghost Engine is live
    ghost_emit_ready();

    // ── Init GPS (Crowtail UART) ──────────────────────────
    ghost_gps_init();

    // ── Init WiFi (scan mode, NOT connected to any AP) ───
    ghost_wifi_init();

    // ── Init BLE scanner ─────────────────────────────────
    ghost_ble_init();

    // ── Init Phase 11 modules ────────────────────────────
    // Each is event-driven; tasks are spawned on demand from
    // the bridge dispatcher (e.g. ghost_netscan_start()) or
    // run continuously from a single _start.
    ghost_promisc_init();
    ghost_http_init();
    ghost_hid_init();
    ghost_ble_gatt_init();
    ghost_netscan_init();
    ghost_rfspectrum_init();
    ghost_wpa_hs_init();
    ghost_wifi_ducky_init();

    // ── Launch collection tasks ───────────────────────────
    // WiFi scan task — Core 0
    xTaskCreatePinnedToCore(
        ghost_wifi_task,
        "ghost_wifi",
        8192,
        NULL,
        5,
        NULL,
        0
    );

    // BLE scan task — Core 1
    xTaskCreatePinnedToCore(
        ghost_ble_task,
        "ghost_ble",
        8192,
        NULL,
        5,
        NULL,
        1
    );

    // GPS parse task — Core 0
    xTaskCreatePinnedToCore(
        ghost_gps_task,
        "ghost_gps",
        4096,
        NULL,
        3,
        NULL,
        0
    );

    // Command listener (P4 → C6 commands) — Core 1
    xTaskCreatePinnedToCore(
        ghost_cmd_task,
        "ghost_cmd",
        4096,
        NULL,
        6,      // Highest priority — commands must be responsive
        NULL,
        1
    );

    ESP_LOGI(TAG, "All Ghost Engine tasks launched.");
    ESP_LOGI(TAG, "Wardriving from boot. Always.");

    // app_main returns — FreeRTOS scheduler takes over
}
