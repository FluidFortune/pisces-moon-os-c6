// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com

#ifndef GHOST_BLE_H
#define GHOST_BLE_H
extern int ghost_ble_devices;
void ghost_ble_init(void);
void ghost_ble_task(void* pvArgs);
#endif
