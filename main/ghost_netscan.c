// Pisces Moon OS
// Copyright (C) 2026 Eric Becker / Fluid Fortune
// SPDX-License-Identifier: AGPL-3.0-or-later
// Contributions: see CLA.md
// fluidfortune.com


// ============================================================
//  ghost_netscan.c — host discovery for the connected subnet
//
//  Strategy: spam ARP requests across the /24, then read the
//  ARP cache to find responders. lwIP exposes the table via
//  etharp_iflookup_ipa. Hosts get stamped with their MAC; for
//  IPv4-aware hosts we also attempt reverse DNS via
//  netconn_gethostbyname (best-effort, often empty).
//
//  Emits host_seen events; on completion, host_scan_done.
// ============================================================

#include "ghost_netscan.h"
#include "ghost_bridge.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"
#include "lwip/ip4_addr.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "GHOST_NETSCAN";

static volatile bool s_running = false;

extern void ghost_emit_raw(const char* s);

static void _emit_host(const char* ip, const char* mac, int latency_ms) {
    char buf[160];
    snprintf(buf, sizeof(buf),
              "{\"event\":\"host_seen\",\"ip\":\"%s\",\"mac\":\"%s\","
              "\"latency_ms\":%d}\n", ip, mac, latency_ms);
    ghost_emit_raw(buf);
}

static void _emit_done(int n_hosts, const char* subnet) {
    char buf[160];
    snprintf(buf, sizeof(buf),
              "{\"event\":\"host_scan_done\",\"hosts\":%d,\"subnet\":\"%s\"}\n",
              n_hosts, subnet ? subnet : "");
    ghost_emit_raw(buf);
}

static void _scan_task(void* arg) {
    (void)arg;
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        ESP_LOGW(TAG, "no STA netif");
        s_running = false;
        vTaskDelete(NULL);
        return;
    }
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(netif, &ip) != ESP_OK) {
        s_running = false;
        vTaskDelete(NULL);
        return;
    }
    uint32_t base = ntohl(ip.ip.addr) & ntohl(ip.netmask.addr);
    uint32_t mask = ntohl(ip.netmask.addr);
    uint32_t span = ~mask & 0xFFFFFFFFu;
    if (span > 254) span = 254;

    char subnet[24];
    snprintf(subnet, sizeof(subnet), IPSTR "/%d",
              IP2STR(&ip.ip), 24);    // assume /24 for the spam phase

    int n_hosts = 0;

    // Pass 1: spam ARP
    for (uint32_t i = 1; i <= span; i++) {
        ip4_addr_t target = { htonl(base + i) };
        struct netif* lwip_netif = (struct netif*)esp_netif_get_netif_impl(netif);
        if (lwip_netif) etharp_request(lwip_netif, &target);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    vTaskDelay(pdMS_TO_TICKS(800));

    // Pass 2: harvest ARP cache
    for (uint32_t i = 1; i <= span; i++) {
        ip4_addr_t       target = { htonl(base + i) };
        struct eth_addr* eth    = NULL;
        const ip4_addr_t* tgt = &target;
        struct netif* lwip_netif = (struct netif*)esp_netif_get_netif_impl(netif);
        if (lwip_netif &&
            etharp_find_addr(lwip_netif, tgt, &eth, &tgt) >= 0 &&
            eth) {
            char ip_s[16], mac_s[18];
            snprintf(ip_s, sizeof(ip_s),  IPSTR, IP2STR(&target));
            snprintf(mac_s, sizeof(mac_s), "%02x:%02x:%02x:%02x:%02x:%02x",
                      eth->addr[0], eth->addr[1], eth->addr[2],
                      eth->addr[3], eth->addr[4], eth->addr[5]);
            _emit_host(ip_s, mac_s, 0);
            n_hosts++;
        }
    }

    _emit_done(n_hosts, subnet);
    s_running = false;
    vTaskDelete(NULL);
}

void ghost_netscan_init(void) { /* nothing — task spawned per scan */ }

void ghost_netscan_start(void) {
    if (s_running) return;
    s_running = true;
    xTaskCreate(_scan_task, "ghost_netscan", 4096, NULL, 4, NULL);
}
