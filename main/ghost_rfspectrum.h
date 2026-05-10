// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_rfspectrum.h — 2.4GHz utilization sweep
//
//  Walks WiFi channels 1..14, measures the noise floor and
//  utilization percentage on each, and emits "channel" events.
//  Repeats indefinitely until rf_spectrum_stop.
//
//  Implementation: enable monitor mode, switch channel each
//  N ms, count matching frames + accumulate RSSI samples.
//  Mutually exclusive with promiscuous/wifi_scan.
// ============================================================

#ifndef GHOST_RFSPECTRUM_H
#define GHOST_RFSPECTRUM_H
void ghost_rfspectrum_init(void);
void ghost_rfspectrum_start(void);
void ghost_rfspectrum_stop (void);
#endif
