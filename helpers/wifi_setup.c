#include <string.h>
#include "wifi_setup.h"
#include "espressif/esp_common.h"
#include <espressif/esp_wifi.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <sntp.h>
#include <time.h>
#include "sysparam.h"
#include "ota-tftp.h"
#include <lwip/api.h>
#include "lwip/etharp.h"
#include "netif/ethernet.h"

#include "ping_helper.h"
#include "debug_helper.h"

#define SNTP_SERVERS 	"0.pool.ntp.org", "1.pool.ntp.org", \
						"2.pool.ntp.org", "3.pool.ntp.org"   

wifi_connected_callback wifi_connected = NULL;
bool ota_update = true;

static void start_sntp() {
    /* Start SNTP */
	printf("Starting SNTP...\n");
	/* SNTP will request an update each 5 minutes */
	sntp_set_update_delay(5*60000);
	/* Set GMT zone, daylight savings off */
	const struct timezone tz = {0, 0};
	/* SNTP initialization */
	sntp_initialize(&tz);
	/* Servers must be configured right after initialization */
    const char *servers[] = {SNTP_SERVERS};
	sntp_set_servers(servers, sizeof(servers) / sizeof(char*));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    time_t ts = time(NULL);
	printf("TIME: %s\n", ctime(&ts));
    setenv("TZ", "", 1);
    tzset();
}

static void on_ping_watchdog_fail_callback() {
    LOG("Ping watchdog failed 30 times. Restart the device");
    sdk_system_restart();
}

static void on_wifi_ready() {
    LOG("");
    start_sntp();
    LOG("Starting TFTP server...");
    if (ota_update) {
        ota_tftp_init_server(TFTP_PORT);
    }
    ip_addr_t to_ping = get_gw_ip();
    start_ping_watchdog(to_ping, 30, 30, on_ping_watchdog_fail_callback);
    if (wifi_connected != NULL) {
        wifi_connected();
    }
}

static void wifi_task(void *_args) {
    LOG("");
    int count = 0;
    while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP && count < 120) {
        count += 1;
        LOG("Waiting for WiFi: %d", count);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    if (count >= 120) {
        LOG("Waited 120 seconds for WiFi connection. Restart the device");
        sdk_system_restart();
    }
    on_wifi_ready();
    vTaskDelete(NULL);
}

void wifi_init(char* ssid, char* password, const char* hostName, bool ota_update_on, wifi_connected_callback callback) {
    LOG("Start WiFi | SSID: %s | Password: %s", ssid, password);
    struct sdk_station_config wifi_config;
    strcpy((char*)(wifi_config.ssid), ssid);
    strcpy((char*)(wifi_config.password), password);

    wifi_connected = callback;
    ota_update = ota_update_on;

    sysparam_set_string("hostname", hostName);
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
    
    xTaskCreate(wifi_task, "WiFi task", 1024, NULL, 1, NULL);
}