// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_wifi_ducky.c — SoftAP + tiny HTTP server
//
//  AP brings up esp_netif_create_default_wifi_ap. The HTTP
//  server uses esp_http_server. One GET serves the form,
//  one POST forwards the body to the P4.
// ============================================================

#include "ghost_wifi_ducky.h"
#include "ghost_bridge.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "GHOST_WIFIDUCKY";
static httpd_handle_t s_server = NULL;

extern void ghost_emit_raw(const char* s);

// Single-page form. Keeps it minimal so it loads on any phone.
static const char FORM_HTML[] =
    "<!DOCTYPE html>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Pisces Ducky</title>"
    "<body style='background:#0A1828;color:#E6F0FA;font:16px sans-serif;padding:20px'>"
    "<h2>Pisces Ducky</h2>"
    "<form method=POST action=/fire>"
    "<textarea name=p style='width:100%;height:240px;background:#122B45;color:#E6F0FA;"
    "border:1px solid #2A4A6C;border-radius:8px;padding:8px'></textarea><br>"
    "<button type=submit style='margin-top:8px;background:#4FD1C5;color:#0A1828;"
    "border:0;padding:10px 20px;border-radius:6px;font-weight:bold'>FIRE</button>"
    "</form></body>";

// GET / → form
static esp_err_t _h_root(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, FORM_HTML, sizeof(FORM_HTML) - 1);
    return ESP_OK;
}

// POST /fire → emit payload to P4
static esp_err_t _h_fire(httpd_req_t* req) {
    int total = req->content_len > 4096 ? 4096 : req->content_len;
    char* buf = (char*)malloc(total + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    int got = httpd_req_recv(req, buf, total);
    if (got <= 0) { free(buf); httpd_resp_send_500(req); return ESP_FAIL; }
    buf[got] = 0;

    // Skip "p=" prefix
    const char* p = strstr(buf, "p=");
    const char* payload = p ? (p + 2) : buf;

    // Emit. URL-encoded form body — keep it simple here, P4 can
    // decode if needed. For most ducky scripts ASCII is fine.
    ghost_emit_raw("{\"event\":\"wifi_ducky_form\",\"payload\":\"");
    ghost_emit_raw(payload);
    ghost_emit_raw("\"}\n");

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "<p>Sent.</p>", -1);
    free(buf);
    return ESP_OK;
}

void ghost_wifi_ducky_init(void) { /* nothing */ }

void ghost_wifi_ducky_ap_start(const char* ssid, const char* pass) {
    if (s_server) return;

    esp_netif_create_default_wifi_ap();
    wifi_config_t wifi_cfg = { .ap = { .max_connection = 4,
                                        .authmode = WIFI_AUTH_WPA2_PSK } };
    strncpy((char*)wifi_cfg.ap.ssid,     ssid ? ssid : "PiscesDucky", 32);
    strncpy((char*)wifi_cfg.ap.password, pass ? pass : "letmeducky",  64);
    wifi_cfg.ap.ssid_len = strlen((char*)wifi_cfg.ap.ssid);
    if (wifi_cfg.ap.ssid_len == 0) wifi_cfg.ap.ssid_len = strlen("PiscesDucky");
    if (strlen((char*)wifi_cfg.ap.password) < 8) wifi_cfg.ap.authmode = WIFI_AUTH_OPEN;

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg);
    esp_wifi_start();

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGW(TAG, "httpd start failed");
        return;
    }
    httpd_uri_t r = { .uri = "/",      .method = HTTP_GET,  .handler = _h_root };
    httpd_uri_t f = { .uri = "/fire",  .method = HTTP_POST, .handler = _h_fire };
    httpd_register_uri_handler(s_server, &r);
    httpd_register_uri_handler(s_server, &f);
    ESP_LOGI(TAG, "AP up: SSID='%s'", (char*)wifi_cfg.ap.ssid);
}

void ghost_wifi_ducky_ap_stop(void) {
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    esp_wifi_stop();
}
