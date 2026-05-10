// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com

#ifndef GHOST_GPS_H
#define GHOST_GPS_H
#include <stdbool.h>

// GPS state — updated by ghost_gps_task from Crowtail UART
void   ghost_gps_init(void);
void   ghost_gps_task(void* pvArgs);
double ghost_gps_lat(void);
double ghost_gps_lng(void);
double ghost_gps_alt(void);
int    ghost_gps_sats(void);
bool   ghost_gps_valid(void);

#endif
