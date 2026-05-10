// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_ble_gatt.h — BLE GATT client for explorer app
//
//  P4 ble_gatt sends:
//    {"cmd":"ble_connect","mac":"AA:BB:CC:DD:EE:FF"}
//    {"cmd":"ble_read","handle":<n>}
//    {"cmd":"ble_write","handle":<n>,"hex":"DEADBEEF"}
//    {"cmd":"ble_disconnect"}
//
//  We connect, do service+char discovery, emit each as
//  ble_service / ble_char events, then route reads/writes
//  by handle.
//
//  Only one peer at a time. Mutually exclusive with HID
//  pairing (both use the BLE radio).
// ============================================================

#ifndef GHOST_BLE_GATT_H
#define GHOST_BLE_GATT_H
void ghost_ble_gatt_init(void);
void ghost_ble_gatt_connect   (const char* mac);
void ghost_ble_gatt_disconnect(void);
void ghost_ble_gatt_read (int handle);
void ghost_ble_gatt_write(int handle, const char* hex);
#endif
