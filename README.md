<!--
Pisces Moon OS
Copyright (C) 2026 Eric Becker / Fluid Fortune
SPDX-License-Identifier: AGPL-3.0-or-later
Contributions: see CLA.md
fluidfortune.com
-->

# Pisces Moon Ghost Engine — ESP32-C6 Firmware

The dedicated radio coprocessor firmware for Pisces Moon OS. This is
a separate ESP-IDF project from the main P4 firmware. They are
flashed independently and run independently; they communicate over
UART using a JSON-framed bridge protocol.

**Companion repo:** `pisces-moon-p4/` (the main P4 firmware)
**License:** AGPL-3.0-or-later
**Status:** Alpha (full firmware tree built, hardware bring-up pending)

---

## What this is

Custom firmware for the **ESP32-C6** coprocessor on the ELECROW
CrowPanel Advanced 7" HMI. It replaces ELECROW's stock C6 firmware
(which is a generic `esp_hosted` coprocessor image) with a dedicated
radio capture daemon: the **Ghost Engine**.

The Ghost Engine continuously scans WiFi and BLE, captures 802.11
frames, decodes EAPOL handshakes, and emits structured events to the
P4 over UART. It does this 100% of the time the device is powered,
regardless of what the user is doing on the P4.

This is the architectural distinction that defines the project: a
dedicated, always-on, non-stoppable radio MCU. The P4 can reboot,
load games, run audio playback — and the C6 just keeps wardriving.

---

## What it does

The C6 listens on UART for JSON commands from the P4 and emits
JSON events back. 25+ commands are dispatched via cJSON:

- **Always-on capture:** `wardrive_start`, `ble_start`,
  `promiscuous_start` (with channel + filter mask)
- **Network control:** `wifi_scan`, `wifi_connect`, `wifi_disconnect`
- **HTTP proxy:** `http_get`, `http_post` (the P4 has no IP stack;
  the C6 fetches and returns base64-encoded responses)
- **BLE HID:** `hid_pair`, `hid_string`, `hid_key` (US-QWERTY keymap)
- **GATT central:** `ble_connect`, `ble_read`, `ble_write` (skeleton)
- **Network scan:** `net_scan` (ARP-sweep host discovery)
- **RF spectrum:** `rf_spectrum_start` (channel utilization sweep)
- **WPA collection:** `wpa_hs_start` (EAPOL frame collector)
- **Captive AP:** `wifi_ducky_ap_start` (browser → form → P4)

Events emitted include: `wifi_seen`, `ble_seen`, `gps` (legacy),
`pkt`, `host_seen`, `channel`, `eapol_seen`, `handshake`,
`http_response`, `ble_service`, `ble_char`, `ble_value`,
`ble_connected`, `ble_disconnected`, `wifi_connected`,
`wifi_disconnected`, `wifi_ducky_form`, `pong`, `status`.

---

## Build

Standard ESP-IDF v5.4.x project. Same setup as the P4 side; see
`pisces-moon-p4/docs/PiscesMoon_P4_BringUp_Guide.docx` for the
toolchain install. Then:

```bash
cd pisces-moon-c6
idf.py set-target esp32c6
idf.py build
```

---

## Flash

**This is the hard part.** The CrowPanel Advanced 7" board does
**not** expose the C6 console pins externally. Direct USB-to-serial
flashing isn't an option without soldering.

The intended flash path is **P4-mediated SDIO flashing**:

1. The P4 acts as the programmer
2. The P4 drives the C6's BOOT/EN pins via SDIO sideband
3. The P4 pushes firmware blocks using the ESP serial bootloader
   protocol tunneled through SDIO
4. From the user's perspective, this looks like opening the
   "C6 Flasher" SYSTEM app on the P4, picking a `.bin` file from
   `/sd/ghost/`, and tapping FLASH

The protocol layer is implemented in the P4-side
`pm_c6_programmer` component. The SDIO transport is currently
stubbed and is the active priority of Phase 14.

Until the SDIO flasher is brought up, you cannot easily put this
firmware onto the chip. ELECROW publishes a "Guide to Upgrading
the C6 Firmware Using the ESP32-P4 Chip" — that's the document
the SDIO transport is being implemented against.

---

## Repository layout

```
pisces-moon-c6/
├── main/
│   ├── ghost_main.c               app_main(), task spawning
│   ├── ghost_bridge.c/.h          UART bridge + cJSON dispatcher
│   ├── ghost_wifi.c/.h            WiFi scanner + STA control
│   ├── ghost_ble.c/.h             BLE scanner
│   ├── ghost_promisc.c/.h         802.11 monitor mode
│   ├── ghost_http.c/.h            HTTP proxy w/ base64 response
│   ├── ghost_hid.c/.h             BLE HID keyboard
│   ├── ghost_ble_gatt.c/.h        BLE GATT central skeleton
│   ├── ghost_netscan.c/.h         ARP-sweep host discovery
│   ├── ghost_rfspectrum.c/.h      channel utilization sweep
│   ├── ghost_wpa_hs.c/.h          EAPOL handshake collector
│   ├── ghost_wifi_ducky.c/.h      captive AP + form server
│   ├── ghost_gps.c/.h             legacy GPS reader (P4 now reads directly)
│   ├── ghost_partition.h
│   ├── c6_bridge.h                shared bridge protocol header
│   └── CMakeLists.txt
├── partitions.csv
├── sdkconfig.defaults
├── CMakeLists.txt
├── CHANGELOG_v1_2_0_alpha.md
├── CLA.md
├── README.md                      this file
└── LICENSE                        AGPL-3.0-or-later
```

---

## Pending work

- **NimBLE central plumbing** in `ghost_ble_gatt.c` (public API
  exists, GATT discovery + read/write callbacks not wired)
- **WPA hccapx assembly** in `ghost_wpa_hs.c` (EAPOL frame capture
  works; binary format synthesis is the open piece)
- **esp_hidd plumbing** in `ghost_hid.c` (keymap + GAP advertising
  exist; the actual HID input report send is stubbed)
- **GPS module** retained for backward compat but the P4 now
  parses NMEA directly from its own UART; the C6 GPS path will
  be removed in a future phase

---

## License

AGPL-3.0-or-later. See `LICENSE`.

Contributions require the CLA in `CLA.md`.

fluidfortune.com
