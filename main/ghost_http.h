// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_http.h — HTTP proxy on the C6
//
//  The P4 side has no WiFi/IP stack. When apps need HTTP
//  (terminal, baseball, trails, voice_terminal, gemini_log),
//  they send a "http_get" / "http_post" command over the
//  bridge; we run it here using esp_http_client and ship the
//  response body back as a single "http_response" event.
//
//  Bodies up to 16 KB are returned in-band as a base64 string.
//  Larger responses are truncated with truncated:true.
//
//  Concurrency: one request at a time. Callers can serialize
//  via a request_id field (echoed back).
// ============================================================

#ifndef GHOST_HTTP_H
#define GHOST_HTTP_H

#include <stdbool.h>
#include <stdint.h>

void ghost_http_init(void);

// Schedule an HTTP GET. Returns immediately; result arrives
// later via ghost_emit_http_response().
//   request_id  — echoed back to caller for matching
//   url         — full URL including scheme
//   bearer_tok  — optional auth token (NULL if none)
void ghost_http_get (uint32_t request_id, const char* url,
                      const char* bearer_tok);

// Schedule an HTTP POST.
//   content_type — e.g. "application/json"
//   body         — raw bytes (NUL-terminated string ok for JSON)
void ghost_http_post(uint32_t request_id, const char* url,
                      const char* bearer_tok,
                      const char* content_type,
                      const char* body, int body_len);

#endif // GHOST_HTTP_H
