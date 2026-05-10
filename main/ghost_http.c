// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_http.c — HTTP proxy worker for the P4
//
//  Single worker task (request_q). Each request runs serially.
//  Body comes back over the bridge UART as a base64-encoded
//  http_response event with request_id for matching.
//
//  Memory: response buffer is allocated per request from the
//  C6 heap, freed on completion. Cap at 16 KB to keep heap
//  fragmentation under control on a 320 KB chip.
// ============================================================

#include "ghost_http.h"
#include "ghost_bridge.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "mbedtls/base64.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char* TAG = "GHOST_HTTP";

#define MAX_BODY_BYTES 16384
#define MAX_URL_LEN     512
#define MAX_HDR_LEN     128

typedef enum { OP_GET, OP_POST } op_t;

typedef struct {
    op_t      op;
    uint32_t  request_id;
    char      url[MAX_URL_LEN];
    char      bearer[MAX_HDR_LEN];
    char      content_type[64];
    int       body_len;
    char*     body;        // owned, free on completion
} req_t;

static QueueHandle_t s_q = NULL;

// Forward
static void _emit_response(uint32_t request_id, int http_status,
                            const uint8_t* data, int data_len,
                            bool truncated);

// ─────────────────────────────────────────────
//  Worker
// ─────────────────────────────────────────────
static esp_err_t _http_event_cb(esp_http_client_event_t* evt) {
    return ESP_OK;
}

static void _do_request(const req_t* r) {
    esp_http_client_config_t cfg = {
        .url           = r->url,
        .timeout_ms    = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = _http_event_cb,
        .buffer_size   = 4096,
    };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    if (!cli) {
        ESP_LOGW(TAG, "init failed for %s", r->url);
        _emit_response(r->request_id, -1, NULL, 0, false);
        return;
    }

    esp_http_client_set_method(cli,
        r->op == OP_GET ? HTTP_METHOD_GET : HTTP_METHOD_POST);

    if (r->bearer[0]) {
        char hdr[160];
        snprintf(hdr, sizeof(hdr), "Bearer %s", r->bearer);
        esp_http_client_set_header(cli, "Authorization", hdr);
    }

    if (r->op == OP_POST) {
        if (r->content_type[0])
            esp_http_client_set_header(cli, "Content-Type", r->content_type);
        if (r->body && r->body_len > 0)
            esp_http_client_set_post_field(cli, r->body, r->body_len);
    }

    esp_err_t err = esp_http_client_open(cli, r->op == OP_POST ? r->body_len : 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open: %s", esp_err_to_name(err));
        _emit_response(r->request_id, -1, NULL, 0, false);
        esp_http_client_cleanup(cli);
        return;
    }

    if (r->op == OP_POST && r->body && r->body_len > 0) {
        int n = esp_http_client_write(cli, r->body, r->body_len);
        if (n < 0) {
            _emit_response(r->request_id, -1, NULL, 0, false);
            esp_http_client_close(cli);
            esp_http_client_cleanup(cli);
            return;
        }
    }

    int content_len = esp_http_client_fetch_headers(cli);
    int status      = esp_http_client_get_status_code(cli);
    (void)content_len;

    uint8_t* buf = (uint8_t*)malloc(MAX_BODY_BYTES);
    if (!buf) {
        _emit_response(r->request_id, status, NULL, 0, false);
        esp_http_client_close(cli);
        esp_http_client_cleanup(cli);
        return;
    }
    int total = 0;
    bool truncated = false;
    while (total < MAX_BODY_BYTES) {
        int n = esp_http_client_read(cli, (char*)buf + total, MAX_BODY_BYTES - total);
        if (n <= 0) break;
        total += n;
    }
    // Drain anything beyond cap to mark truncation
    {
        char dump[256];
        int n = esp_http_client_read(cli, dump, sizeof(dump));
        if (n > 0) truncated = true;
        while (n > 0) n = esp_http_client_read(cli, dump, sizeof(dump));
    }

    _emit_response(r->request_id, status, buf, total, truncated);
    free(buf);

    esp_http_client_close(cli);
    esp_http_client_cleanup(cli);
}

static void _http_task(void* arg) {
    (void)arg;
    req_t r;
    for (;;) {
        if (xQueueReceive(s_q, &r, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "%s %s", r.op == OP_GET ? "GET" : "POST", r.url);
            _do_request(&r);
            if (r.body) free(r.body);
        }
    }
}

// ─────────────────────────────────────────────
//  Init
// ─────────────────────────────────────────────
void ghost_http_init(void) {
    s_q = xQueueCreate(4, sizeof(req_t));
    xTaskCreate(_http_task, "ghost_http", 8192, NULL, 5, NULL);
    ESP_LOGI(TAG, "HTTP proxy ready");
}

void ghost_http_get(uint32_t request_id, const char* url, const char* bearer) {
    if (!s_q || !url) return;
    req_t r = {0};
    r.op = OP_GET;
    r.request_id = request_id;
    strncpy(r.url, url, sizeof(r.url) - 1);
    if (bearer) strncpy(r.bearer, bearer, sizeof(r.bearer) - 1);
    xQueueSend(s_q, &r, 0);
}

void ghost_http_post(uint32_t request_id, const char* url,
                      const char* bearer, const char* content_type,
                      const char* body, int body_len) {
    if (!s_q || !url) return;
    req_t r = {0};
    r.op = OP_POST;
    r.request_id = request_id;
    strncpy(r.url, url, sizeof(r.url) - 1);
    if (bearer)       strncpy(r.bearer,       bearer,       sizeof(r.bearer)       - 1);
    if (content_type) strncpy(r.content_type, content_type, sizeof(r.content_type) - 1);
    if (body && body_len > 0) {
        r.body     = (char*)malloc(body_len + 1);
        if (r.body) {
            memcpy(r.body, body, body_len);
            r.body[body_len] = 0;
            r.body_len = body_len;
        }
    }
    xQueueSend(s_q, &r, 0);
}

// ─────────────────────────────────────────────
//  Response → bridge UART
// ─────────────────────────────────────────────
static void _emit_response(uint32_t request_id, int http_status,
                            const uint8_t* data, int data_len,
                            bool truncated) {
    // base64 encode body. Output buffer = ((n+2)/3)*4 + 1.
    size_t out_max = ((data_len + 2) / 3) * 4 + 1;
    char* b64 = (char*)malloc(out_max + 64);
    if (!b64) return;

    size_t out_len = 0;
    if (data && data_len > 0) {
        if (mbedtls_base64_encode((unsigned char*)b64, out_max, &out_len,
                                   data, data_len) != 0) {
            out_len = 0;
            b64[0]  = 0;
        } else {
            b64[out_len] = 0;
        }
    } else {
        b64[0] = 0;
    }

    // Emit. Body is wrapped in quotes; base64 has no embedded
    // double quotes so this is safe.
    char hdr[128];
    snprintf(hdr, sizeof(hdr),
             "{\"event\":\"http_response\",\"id\":%u,\"status\":%d,"
             "\"truncated\":%s,\"len\":%d,\"body_b64\":\"",
             (unsigned)request_id, http_status,
             truncated ? "true" : "false", data_len);
    extern void ghost_emit_raw(const char* s);
    ghost_emit_raw(hdr);
    ghost_emit_raw(b64);
    ghost_emit_raw("\"}\n");

    free(b64);
}
