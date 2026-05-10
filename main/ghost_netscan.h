// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_netscan.h — subnet host discovery (ARP + ICMP)
//
//  When the C6 is associated to an SSID, this sweeps the
//  /24 it sits on, sending ARP-who-has and ping packets.
//  Results emit as host_seen events.
//
//  Async: returns immediately, work runs in a worker task.
//  Caller can issue net_scan again to restart.
// ============================================================

#ifndef GHOST_NETSCAN_H
#define GHOST_NETSCAN_H
void ghost_netscan_init(void);
void ghost_netscan_start(void);
#endif // GHOST_NETSCAN_H
