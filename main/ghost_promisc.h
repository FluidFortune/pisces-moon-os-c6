// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_promisc.h — 802.11 promiscuous capture
//
//  Builds on esp_wifi_set_promiscuous(). The C6 has an
//  802.11 monitor mode capable of management/control/data
//  frame capture.
//
//  Control:
//    {"cmd":"promiscuous_start"}              — all frame types
//    {"cmd":"promiscuous_filter","mask":3}    — bitmask 1=mgmt 2=data 4=ctrl
//    {"cmd":"promiscuous_stop"}
//
//  Events emitted to P4:
//    {"event":"pkt","frame_type":"mgmt","src":"...", ...}
//
//  Mutually exclusive with wifi_scan / wifi_connect — those
//  paths automatically pause promiscuous and resume after.
// ============================================================

#ifndef GHOST_PROMISC_H
#define GHOST_PROMISC_H

#include <stdbool.h>
#include <stdint.h>

void ghost_promisc_init(void);

void ghost_promisc_start(void);
void ghost_promisc_stop (void);
void ghost_promisc_set_filter(uint8_t mask);
bool ghost_promisc_is_active(void);

#endif // GHOST_PROMISC_H
