// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_wifi_ducky.h — Captive AP for the WiFi Ducky app
//
//  Hosts a SoftAP + tiny HTTP server. Browser hits any URL,
//  gets a captive-portal redirect to a single-page form. The
//  form posts a payload → we relay it back to the P4 over the
//  bridge as a wifi_ducky_form event. The P4 then replays it
//  over USB HID.
// ============================================================

#ifndef GHOST_WIFI_DUCKY_H
#define GHOST_WIFI_DUCKY_H
void ghost_wifi_ducky_init(void);
void ghost_wifi_ducky_ap_start(const char* ssid, const char* pass);
void ghost_wifi_ducky_ap_stop (void);
#endif
