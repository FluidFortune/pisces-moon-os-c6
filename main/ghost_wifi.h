// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com

#ifndef GHOST_WIFI_H
#define GHOST_WIFI_H

// ============================================================
//  ghost_wifi.h — WiFi scanning + STA control
//
//  The Ghost Engine wifi task runs continuously (when active)
//  and emits wifi_seen events. The functions below are called
//  from the bridge dispatcher when the P4 wants to drive STA
//  associations explicitly.
// ============================================================

extern int ghost_networks_found;

void ghost_wifi_init(void);
void ghost_wifi_task(void* pvArgs);

// STA control — called from bridge dispatcher
void ghost_wifi_scan_request(void);
void ghost_wifi_connect(const char* ssid, const char* pass);
void ghost_wifi_disconnect_request(void);
void ghost_wifi_emit_status(void);

#endif
