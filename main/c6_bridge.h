// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  c6_bridge.h — P4 side receiver for C6 Ghost Engine stream
//
//  The P4 treats the C6 exactly like a connected T-Deck in
//  Bridge mode. Same JSON parsing. Same event dispatch.
//  Same protocol. Different physical connection (UART vs USB).
//
//  UART: UART2 on P4 side (maps to C6's UART1)
//  Baud: 921600
// ============================================================

#ifndef C6_BRIDGE_H
#define C6_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

#define C6_BRIDGE_UART_NUM  2       // P4 UART2 → C6 UART1
#define C6_BRIDGE_BAUD      921600
#define C6_BRIDGE_TX_PIN    XX      // TBD — from Eagle schematic
#define C6_BRIDGE_RX_PIN    XX      // TBD — from Eagle schematic

// ── Event callbacks (implement in pisces_moon_p4_main.c) ──────────
// Called when C6 emits a wifi_seen event
typedef void (*c6_wifi_cb)(const char* mac, const char* ssid,
                             int rssi, int ch, const char* enc,
                             double lat, double lng);

// Called when C6 emits a ble_seen event
typedef void (*c6_ble_cb)(const char* mac, const char* name,
                            int rssi, double lat, double lng,
                            const char* addr_type,
                            const char* mfg_data);

// Called when C6 emits a probe_seen event
typedef void (*c6_probe_cb)(const char* mac, const char* ssid,
                              int rssi, int count,
                              double lat, double lng);

// Called when C6 emits a gps event
typedef void (*c6_gps_cb)(double lat, double lng, double alt,
                            int sats, bool valid);

// Called when C6 emits a pkt event
typedef void (*c6_pkt_cb)(const char* frame_type,
                            const char* src, int rssi);

// ── Init ─────────────────────────────────────────────────
void c6_bridge_init(c6_wifi_cb  on_wifi,
                     c6_ble_cb   on_ble,
                     c6_probe_cb on_probe,
                     c6_gps_cb   on_gps,
                     c6_pkt_cb   on_pkt);

// ── Commands to C6 ────────────────────────────────────────
void c6_cmd_wardrive_start(void);
void c6_cmd_wardrive_stop(void);
void c6_cmd_ble_start(void);
void c6_cmd_ble_stop(void);
void c6_cmd_promiscuous_start(void);
void c6_cmd_promiscuous_stop(void);
void c6_cmd_raw_log_start(void);
void c6_cmd_raw_log_stop(void);
void c6_cmd_ping(void);
void c6_cmd_status(void);

// ── Receiver task (call from xTaskCreate) ────────────────
void c6_bridge_task(void* pvArgs);

// ── State ─────────────────────────────────────────────────
extern volatile bool c6_connected;      // C6 sent ready event
extern volatile bool c6_wardrive_active;
extern volatile bool c6_ble_active;

#endif // C6_BRIDGE_H
