//
//  wifi_scan.c
//  xcode
//
//  Created by Łukasz Śliwiński on 28/11/2019.
//  Copyright © 2019 plum. All rights reserved.
//

#include "wifi_scan.h"

#include <esp/uart.h>
#include <stdio.h>
#include <espressif/esp_common.h>
#include <FreeRTOS.h>
#include <task.h>
#include <string.h>

#include <lwip/api.h>
#include <lwip/err.h>
#include <lwip/sockets.h>

#include "garage_debug.h"

typedef struct _wifi_scan {
    char expected_ssid[33];
    wifi_scan_callback_fn callback;
    TaskHandle_t xHandle;
} wifi_scan_t;

wifi_scan_t *wifi_scan_observer = NULL;

static const char * const auth_modes [] = {
    [AUTH_OPEN]         = "Open",
    [AUTH_WEP]          = "WEP",
    [AUTH_WPA_PSK]      = "WPA/PSK",
    [AUTH_WPA2_PSK]     = "WPA2/PSK",
    [AUTH_WPA_WPA2_PSK] = "WPA/WPA2/PSK"
};

static void scan_done_cb(void *arg, sdk_scan_status_t status) {
    char ssid[33]; // max SSID length + zero byte

    if (status != SCAN_OK) {
        LOG("### Error: WiFi scan failed");
        return;
    }

    struct sdk_bss_info *bss = (struct sdk_bss_info *)arg;
    // first one is invalid
    bss = bss->next.stqe_next;

    LOG("### Wi-Fi networks");

    bool found_ssid = false;
    bool socket_connected = false;
    
    while (NULL != bss) {
        size_t len = strlen((const char *)bss->ssid);
        memcpy(ssid, bss->ssid, len);
        ssid[len] = 0;

        LOG("%32s (" MACSTR ") RSSI: %02d, security: %s\n", ssid,
            MAC2STR(bss->bssid), bss->rssi, auth_modes[bss->authmode]);
        
        if (strcmp(ssid, wifi_scan_observer->expected_ssid) == 0) {
            found_ssid = true;
        }

        bss = bss->next.stqe_next;
    }
    LOG("# END Wi-Fi networks");
    
    struct sockaddr_in serv_addr;
    
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        LOG("Failed to allocate a socket");
    }

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_port = htons(23);
    serv_addr.sin_family = AF_INET;

    if (inet_aton("192.168.20.1", &serv_addr.sin_addr.s_addr) == false) {
        LOG("Failed to set IP address");
    }
    
    if (connect(s, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
        LOG("Socket connection failed");
    } else {
        socket_connected = true;
    }
    
    wifi_scan_observer->callback(found_ssid, socket_connected);
}

static void scan_task(void *arg) {
    while (true) {
        sdk_wifi_station_scan(NULL, scan_done_cb);
        vTaskDelay(15000 / portTICK_PERIOD_MS);
    }
}

void start_wifi_scan(char *ssid, wifi_scan_callback_fn callback) {
    if (wifi_scan_observer) {
        return;
    }
    
    wifi_scan_observer = malloc(sizeof(wifi_scan_t));
    memset(wifi_scan_observer, 0, sizeof(*wifi_scan_observer));
    strncpy(wifi_scan_observer->expected_ssid,
            ssid,
            sizeof(wifi_scan_observer->expected_ssid));
    wifi_scan_observer->callback = callback;
    wifi_scan_observer->xHandle = NULL;
    xTaskCreate(scan_task, "scan", 512, NULL, 2, &wifi_scan_observer->xHandle);
}

void stop_wifi_scan() {
    if (wifi_scan_observer) {
        vTaskDelete(wifi_scan_observer->xHandle);
        free(wifi_scan_observer->expected_ssid);
        free(wifi_scan_observer);
        wifi_scan_observer = NULL;
    }
}
