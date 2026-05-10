# Pisces Moon OS
# Copyright (C) 2026 Eric Becker / Fluid Fortune
# SPDX-License-Identifier: AGPL-3.0-or-later
# Contributions: see CLA.md
# fluidfortune.com

idf_component_register(
    SRCS
        "ghost_main.c"
        "ghost_bridge.c"
        "ghost_wifi.c"
        "ghost_ble.c"
        "ghost_gps.c"
        "ghost_promisc.c"
        "ghost_http.c"
        "ghost_hid.c"
        "ghost_ble_gatt.c"
        "ghost_netscan.c"
        "ghost_rfspectrum.c"
        "ghost_wpa_hs.c"
        "ghost_wifi_ducky.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_event
        esp_netif
        esp_wifi
        esp_http_client
        esp_http_server
        esp-tls
        bt
        driver
        json
        mbedtls
)
