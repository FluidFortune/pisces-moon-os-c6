// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_gps.c — NMEA GPS parser
//
//  Reads NMEA sentences from GPS module on Crowtail UART.
//  Parses $GPRMC and $GPGGA for lat/lng/alt/sats/valid.
//  GPS coordinates injected into all wifi_seen/ble_seen events.
//
//  UART: UART2 on C6 (Crowtail UART port)
//  Baud: 9600 (standard GPS NMEA)
//  Pin mapping: TBD — verify from board schematic
// ============================================================

#include "ghost_gps.h"
#include "ghost_bridge.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char* TAG = "GHOST_GPS";

// GPS UART — separate from Bridge UART
#define GPS_UART_NUM  2
#define GPS_TX_PIN    4    // TBD — verify from schematic
#define GPS_RX_PIN    5    // TBD — verify from schematic
#define GPS_BAUD      9600

// GPS state
static volatile double _lat   = 0.0;
static volatile double _lng   = 0.0;
static volatile double _alt   = 0.0;
static volatile int    _sats  = 0;
static volatile bool   _valid = false;

// ── NMEA coordinate conversion ───────────────────────────
// NMEA format: DDDMM.MMMMM → decimal degrees
static double _nmea_to_dec(const char* nmea, char hemi) {
    if (!nmea || nmea[0] == 0) return 0.0;
    double raw = atof(nmea);
    int deg = (int)(raw / 100);
    double min = raw - deg * 100.0;
    double dec = deg + min / 60.0;
    if (hemi == 'S' || hemi == 'W') dec = -dec;
    return dec;
}

// ── NMEA sentence parser ─────────────────────────────────
static void _parse_nmea(const char* sentence) {
    // $GPRMC,HHMMSS,A/V,LLLL.LL,N/S,LLLLL.LL,E/W,speed,hdg,date,...
    if (strncmp(sentence, "$GPRMC", 6) == 0 ||
        strncmp(sentence, "$GNRMC", 6) == 0) {
        char buf[128];
        strncpy(buf, sentence, 127);
        char* tok = strtok(buf, ",");
        char fields[15][32] = {};
        int n = 0;
        while (tok && n < 15) {
            strncpy(fields[n++], tok, 31);
            tok = strtok(NULL, ",");
        }
        if (n >= 7) {
            _valid = (fields[2][0] == 'A');
            _lat = _nmea_to_dec(fields[3], fields[4][0]);
            _lng = _nmea_to_dec(fields[5], fields[6][0]);
        }
    }

    // $GPGGA,HHMMSS,LLLL.LL,N/S,LLLLL.LL,E/W,fix,sats,hdop,alt,...
    if (strncmp(sentence, "$GPGGA", 6) == 0 ||
        strncmp(sentence, "$GNGGA", 6) == 0) {
        char buf[128];
        strncpy(buf, sentence, 127);
        char* tok = strtok(buf, ",");
        char fields[15][32] = {};
        int n = 0;
        while (tok && n < 15) {
            strncpy(fields[n++], tok, 31);
            tok = strtok(NULL, ",");
        }
        if (n >= 10) {
            _sats = atoi(fields[7]);
            _alt  = atof(fields[9]);
        }
    }
}

// ── Public accessors ─────────────────────────────────────
double ghost_gps_lat(void)  { return _lat; }
double ghost_gps_lng(void)  { return _lng; }
double ghost_gps_alt(void)  { return _alt; }
int    ghost_gps_sats(void) { return _sats; }
bool   ghost_gps_valid(void){ return _valid; }

// ── Init ─────────────────────────────────────────────────
void ghost_gps_init(void) {
    uart_config_t cfg = {
        .baud_rate  = GPS_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(GPS_UART_NUM, &cfg);
    uart_set_pin(GPS_UART_NUM,
                 GPS_TX_PIN, GPS_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(GPS_UART_NUM, 512, 0, 0, NULL, 0);
    ESP_LOGI(TAG, "GPS UART init: %d baud TX=%d RX=%d",
             GPS_BAUD, GPS_TX_PIN, GPS_RX_PIN);
}

// ── GPS task ─────────────────────────────────────────────
void ghost_gps_task(void* pvArgs) {
    uint8_t rxbuf[128];
    char    linebuf[128];
    int     linelen = 0;
    uint32_t last_emit = 0;

    ESP_LOGI(TAG, "GPS task running");

    while (1) {
        int len = uart_read_bytes(GPS_UART_NUM, rxbuf,
                                   sizeof(rxbuf) - 1,
                                   pdMS_TO_TICKS(100));
        for (int i = 0; i < len; i++) {
            char c = (char)rxbuf[i];
            if (c == '\n' || linelen >= 126) {
                linebuf[linelen] = 0;
                linelen = 0;
                if (linebuf[0] == '$') {
                    _parse_nmea(linebuf);

                    // Emit GPS event every 5 seconds
                    uint32_t now = xTaskGetTickCount() *
                                   portTICK_PERIOD_MS;
                    if (now - last_emit > 5000) {
                        ghost_emit_gps(_lat, _lng, _alt,
                                        _sats, _valid, 0.0);
                        last_emit = now;
                    }
                }
            } else if (c != '\r') {
                linebuf[linelen++] = c;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
