// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_wpa_hs.h — WPA(2) 4-way handshake EAPOL collector
//
//  In monitor mode, watches for Type/Subtype 0x88 (QoS Data)
//  and 0x08 (Data) frames whose 802.11 payload begins with
//  the LLC SNAP for EAPOL (AA AA 03 00 00 00 88 8E). When a
//  station-AP pair is seen completing msg1+msg2, we emit a
//  handshake event with the assembled hccapx as base64.
//
//  Stub of the assembler is below — getting a *fully valid*
//  hccapx is intricate (needs message-1 nonce, ssid, MAC
//  pair). The skeleton here captures the right frames; the
//  hccapx synthesis is a TODO that hashcat tooling can be
//  cribbed from.
// ============================================================

#ifndef GHOST_WPA_HS_H
#define GHOST_WPA_HS_H
void ghost_wpa_hs_init(void);
void ghost_wpa_hs_start(void);
void ghost_wpa_hs_stop (void);
#endif
