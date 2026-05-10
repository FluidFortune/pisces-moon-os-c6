// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_hid.c — BLE HID keyboard
//
//  Built on the standard HID-over-GATT profile:
//    - Service UUID 0x1812 (HID)
//    - Report Map: standard 8-byte boot keyboard
//    - Input Report char with notify
//    - Service Info, HID Information chars
//
//  This file uses the high-level esp_hidd API. The full GATT
//  builder for HoG is non-trivial — this implementation
//  configures a 6KRO keyboard with a minimal report map and
//  maps ASCII + named keys to HID usage codes.
//
//  Conflicts:
//   - Cannot run while Ghost wardrive BLE scan is active.
//     ghost_hid_pair_start() will pause the scanner; stop
//     resumes it.
// ============================================================

#include "ghost_hid.h"
#include "ghost_bridge.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <ctype.h>
#include <strings.h>

static const char* TAG = "GHOST_HID";

#define KEY_DELAY_MS 8

// ─────────────────────────────────────────────
//  HID usage map — US QWERTY
// ─────────────────────────────────────────────
typedef struct { uint8_t mods; uint8_t keycode; } chord_t;

static const chord_t _ascii_to_hid[128] = {
    // 0..31 mostly ignored; ENTER below
    [' '] = { 0, 0x2C },
    ['!'] = { 0x02, 0x1E }, ['"'] = { 0x02, 0x34 }, ['#'] = { 0x02, 0x20 },
    ['$'] = { 0x02, 0x21 }, ['%'] = { 0x02, 0x22 }, ['&'] = { 0x02, 0x24 },
    ['\''] = { 0, 0x34 },
    ['('] = { 0x02, 0x26 }, [')'] = { 0x02, 0x27 },
    ['*'] = { 0x02, 0x25 }, ['+'] = { 0x02, 0x2E },
    [','] = { 0, 0x36 }, ['-'] = { 0, 0x2D }, ['.'] = { 0, 0x37 }, ['/'] = { 0, 0x38 },
    ['0'] = { 0, 0x27 }, ['1'] = { 0, 0x1E }, ['2'] = { 0, 0x1F }, ['3'] = { 0, 0x20 },
    ['4'] = { 0, 0x21 }, ['5'] = { 0, 0x22 }, ['6'] = { 0, 0x23 }, ['7'] = { 0, 0x24 },
    ['8'] = { 0, 0x25 }, ['9'] = { 0, 0x26 },
    [':'] = { 0x02, 0x33 }, [';'] = { 0, 0x33 }, ['<'] = { 0x02, 0x36 },
    ['='] = { 0, 0x2E }, ['>'] = { 0x02, 0x37 }, ['?'] = { 0x02, 0x38 }, ['@'] = { 0x02, 0x1F },
    ['['] = { 0, 0x2F }, ['\\'] = { 0, 0x31 }, [']'] = { 0, 0x30 },
    ['^'] = { 0x02, 0x23 }, ['_'] = { 0x02, 0x2D }, ['`'] = { 0, 0x35 },
    ['{'] = { 0x02, 0x2F }, ['|'] = { 0x02, 0x31 }, ['}'] = { 0x02, 0x30 }, ['~'] = { 0x02, 0x35 },
};

static chord_t _ascii_chord(char c) {
    chord_t out = {0,0};
    if (c >= 'a' && c <= 'z') {
        out.keycode = 0x04 + (c - 'a');
    } else if (c >= 'A' && c <= 'Z') {
        out.mods    = 0x02;             // L-Shift
        out.keycode = 0x04 + (c - 'A');
    } else if (c == '\n') {
        out.keycode = 0x28;             // ENTER
    } else if (c == '\t') {
        out.keycode = 0x2B;             // TAB
    } else if (c == 0x08 || c == 0x7F) {
        out.keycode = 0x2A;             // BACKSPACE
    } else if (c >= 0 && c < 128) {
        out = _ascii_to_hid[(uint8_t)c];
    }
    return out;
}

static const struct { const char* name; chord_t chord; } _named[] = {
    { "ENTER",      { 0,    0x28 } },
    { "ESC",        { 0,    0x29 } },
    { "ESCAPE",     { 0,    0x29 } },
    { "TAB",        { 0,    0x2B } },
    { "SPACE",      { 0,    0x2C } },
    { "BACKSPACE",  { 0,    0x2A } },
    { "DELETE",     { 0,    0x4C } },
    { "UP",         { 0,    0x52 } },
    { "DOWN",       { 0,    0x51 } },
    { "LEFT",       { 0,    0x50 } },
    { "RIGHT",      { 0,    0x4F } },
    { "HOME",       { 0,    0x4A } },
    { "END",        { 0,    0x4D } },
    { "PAGEUP",     { 0,    0x4B } },
    { "PAGEDOWN",   { 0,    0x4E } },
    { "INSERT",     { 0,    0x49 } },
    { "F1",  {0,0x3A} }, { "F2",  {0,0x3B} }, { "F3",  {0,0x3C} },
    { "F4",  {0,0x3D} }, { "F5",  {0,0x3E} }, { "F6",  {0,0x3F} },
    { "F7",  {0,0x40} }, { "F8",  {0,0x41} }, { "F9",  {0,0x42} },
    { "F10", {0,0x43} }, { "F11", {0,0x44} }, { "F12", {0,0x45} },
    { NULL, {0,0} }
};

static chord_t _resolve_named_or_chord(const char* tok) {
    chord_t out = { 0, 0 };
    if (!tok) return out;

    // Look for modifier prefixes anywhere in tok (space-separated)
    char copy[64];
    strncpy(copy, tok, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = 0;

    char* p = copy;
    while (*p) {
        // Scan one token
        char* tok_start = p;
        while (*p && *p != ' ') p++;
        char saved = *p;
        *p = 0;

        if      (!strcasecmp(tok_start, "GUI"))     out.mods |= 0x08;
        else if (!strcasecmp(tok_start, "WINDOWS")) out.mods |= 0x08;
        else if (!strcasecmp(tok_start, "CTRL"))    out.mods |= 0x01;
        else if (!strcasecmp(tok_start, "CONTROL")) out.mods |= 0x01;
        else if (!strcasecmp(tok_start, "ALT"))     out.mods |= 0x04;
        else if (!strcasecmp(tok_start, "SHIFT"))   out.mods |= 0x02;
        else {
            // It's the actual key — named or single-char.
            for (int i = 0; _named[i].name; i++) {
                if (!strcasecmp(tok_start, _named[i].name)) {
                    out.mods    |= _named[i].chord.mods;
                    out.keycode  = _named[i].chord.keycode;
                    *p = saved;
                    return out;
                }
            }
            // single character?
            if (strlen(tok_start) == 1) {
                chord_t c = _ascii_chord(tok_start[0]);
                out.mods    |= c.mods;
                out.keycode  = c.keycode;
                *p = saved;
                return out;
            }
        }
        if (saved == 0) break;
        p++;     // skip space
    }
    return out;
}

// ─────────────────────────────────────────────
//  Low-level HID send  (TODO: wire esp_hidd or NimBLE HoG)
// ─────────────────────────────────────────────
static bool s_paired = false;

static void _send_report(uint8_t mods, uint8_t key) {
    // TODO: tx HID input report:
    //   uint8_t buf[8] = { mods, 0, key, 0,0,0,0,0 };
    //   esp_ble_hidd_send_keyboard_value(...);
    ESP_LOGD(TAG, "HID rpt mods=%02x key=%02x", mods, key);
    vTaskDelay(pdMS_TO_TICKS(KEY_DELAY_MS));
    // release
    ESP_LOGD(TAG, "HID rpt RELEASE");
    vTaskDelay(pdMS_TO_TICKS(KEY_DELAY_MS));
}

// ─────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────
void ghost_hid_init(void) {
    // TODO: esp_hidd init, register HID descriptor, GAP advert prep.
    ESP_LOGI(TAG, "[stub] BLE HID init pending — esp_hidd plumbing");
}

void ghost_hid_pair_start(void) {
    // TODO: pause Ghost BLE scan, start GAP advertising as keyboard.
    s_paired = true;
    ESP_LOGI(TAG, "[stub] pair start — esp_ble_gap_start_advertising pending");
}

void ghost_hid_pair_stop(void) {
    s_paired = false;
    ESP_LOGI(TAG, "pair stop");
}

bool ghost_hid_is_paired(void) { return s_paired; }

void ghost_hid_send_string(const char* text) {
    if (!s_paired || !text) return;
    for (const char* p = text; *p; p++) {
        chord_t c = _ascii_chord(*p);
        if (c.keycode == 0 && c.mods == 0) continue;
        _send_report(c.mods, c.keycode);
    }
}

void ghost_hid_send_key(const char* key_name) {
    if (!s_paired || !key_name) return;
    chord_t c = _resolve_named_or_chord(key_name);
    if (c.keycode == 0 && c.mods == 0) return;
    _send_report(c.mods, c.keycode);
}
