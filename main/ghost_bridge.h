// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


#ifndef GHOST_BRIDGE_H
#define GHOST_BRIDGE_H

// ============================================================
//  ghost_bridge.h — C6 → P4 JSON event protocol
//
//  The C6 Ghost Engine speaks the same Bridge JSON protocol
//  as the T-Deck Bridge app. The P4 receives it identically
//  to how pm_bridge.py receives it from the T-Deck.
//
//  UART: UART1 at 921600 baud (P4 side: UART2)
//  Format: one JSON object per line, terminated with \n
//
//  Events emitted (C6 → P4):
//    wifi_seen, ble_seen, probe_seen, pkt, gps, ready, status
//
//  Commands received (P4 → C6):
//    wardrive_start, wardrive_stop, ble_start, ble_stop,
//    promiscuous_start, promiscuous_stop,
//    raw_log_start, raw_log_stop, ping, status
// ============================================================

#include <stdbool.h>
#include <stdint.h>

#define GHOST_BRIDGE_BAUD     921600
#define GHOST_BRIDGE_UART_NUM 1        // C6 UART1 → P4 UART2
#define GHOST_BRIDGE_TX_PIN   16       // TBD — verify from schematic
#define GHOST_BRIDGE_RX_PIN   17       // TBD — verify from schematic
#define GHOST_VERSION         "1.0.0"
#define GHOST_FIRMWARE_NAME   "GhostEngine-P4-C6"

// ── State flags (set by command handler) ─────────────────
extern volatile bool ghost_wardrive_active;
extern volatile bool ghost_ble_active;
extern volatile bool ghost_promiscuous_active;
extern volatile bool ghost_raw_log;
extern volatile bool ghost_bridge_streaming;

// ── Init ─────────────────────────────────────────────────
void ghost_bridge_init(void);

// ── Emit helpers ─────────────────────────────────────────
void ghost_emit_wifi_seen(const char* mac, const char* ssid,
                           int rssi, int ch, const char* enc,
                           double lat, double lng);

void ghost_emit_ble_seen(const char* mac, const char* name,
                          int rssi, double lat, double lng,
                          const char* addr_type, const char* mfg_hex);

void ghost_emit_probe_seen(const char* mac, const char* ssid,
                            int rssi, int count,
                            double lat, double lng);

void ghost_emit_pkt(const char* frame_type, const char* src,
                     const char* dst, const char* bssid,
                     const char* ssid, int ch, int rssi);

void ghost_emit_gps(double lat, double lng, double alt,
                     int sats, bool valid, double speed);

void ghost_emit_ready(void);
void ghost_emit_status(void);

// ── Command task ─────────────────────────────────────────
void ghost_cmd_task(void* pvArgs);

#endif // GHOST_BRIDGE_H
