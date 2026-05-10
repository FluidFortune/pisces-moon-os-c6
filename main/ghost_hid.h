// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_hid.h — BLE HID keyboard service (C6 → host)
//
//  P4 apps ble_ducky and the gamepad-pairing flow drive HID
//  through here:
//
//    {"cmd":"hid_pair"}                — start advertising
//    {"cmd":"hid_string","text":"..."} — type literal text
//    {"cmd":"hid_key","key":"ENTER"}   — single key (or chord)
//    {"cmd":"hid_disconnect"}          — stop advertising
//
//  Internally maps each character of text or key name to a
//  USB HID usage code, then sends a series of input reports
//  through esp_hidd. The keymap is US-QWERTY by default.
// ============================================================

#ifndef GHOST_HID_H
#define GHOST_HID_H

#include <stdbool.h>
#include <stdint.h>

void ghost_hid_init(void);

void ghost_hid_pair_start(void);
void ghost_hid_pair_stop(void);
bool ghost_hid_is_paired(void);

void ghost_hid_send_string(const char* text);
void ghost_hid_send_key   (const char* key_name);

#endif // GHOST_HID_H
