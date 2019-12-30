#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <assert.h>
#include <etstimer.h>
#include <esplibs/libmain.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <udplogger.h>
#include <sntp.h>
#include <time.h>

#include <lwip/api.h>
#include "lwip/etharp.h"
#include "netif/ethernet.h"
#include "sysparam.h"

#include "ota-tftp.h"
#include "rboot-api.h"

#include "wifi.h"
#include <wifi_scan.h>
#include <debug_helper.h>
#include <interrupt_gpio.h>
#include <button_sensor.h>
#include <ping_helper.h>

#define SNTP_SERVERS 	"0.pool.ntp.org", "1.pool.ntp.org", \
						"2.pool.ntp.org", "3.pool.ntp.org"      

// Possible values for characteristic CURRENT_DOOR_STATE:
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN 0
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED 1
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING 2
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING 3
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_STOPPED 4
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_UNKNOWN 255

#define HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_OPEN 0
#define HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED 1
#define HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_UNKNOWN 255

#define OPEN_CLOSE_DURATION 19
#define LAMP_ON_DURATION 120
#define RELAY_MS_DURATION 400

#ifndef MAX_PING_FAILURE_COUNT
#define MAX_PING_FAILURE_COUNT 30
#endif

bool contact_sensor_state_get(uint8_t gpio_num) {
    return interrupt_gpio_state_get(gpio_num);
}

const char *state_description(uint8_t state) {
    switch (state) {
        case HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN: return "open";
        case HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING: return "opening";
        case HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED: return "closed";
        case HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING: return "closing";
        case HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_STOPPED: return "stopped";
        default: return "unknown";
    }
}

void start_ping();

//// GPIO setup
static void led_write(bool on) {
    LOG("Led write: %s. (org: %s)", boolToString(!on), boolToString(on));
    gpio_write(LED_PIN, !on); // ESP8266MOD has LED configured active LOW
}

static void relay_write(bool on) {
    LOG("Relay write: %s.", boolToString(on));
    gpio_write(RELAY_PIN, on);
}

static void lamp_write(bool on) {
    LOG("Lamp write: %s.", boolToString(on));
    gpio_write(LAMP_PIN, on);
}

static void gpio_init() {
    LOG("Using LED at GPIO%d.", LED_PIN);
    gpio_enable(LED_PIN, GPIO_OUTPUT);
    led_write(false);
    LOG("Using RELAY at GPIO%d.", RELAY_PIN);
    gpio_enable(RELAY_PIN, GPIO_OUTPUT);
    relay_write(false);
    LOG("Using LAMP at GPIO%d.", LAMP_PIN);
    gpio_enable(LAMP_PIN, GPIO_OUTPUT);
    lamp_write(false);
}

//// Garage lamp ///////////////////////////////////////////////////////////
bool _lamp_on = false;
ETSTimer lamp_timer; // used for delayed switch off lamp
void lamp_state_set(bool on);

// Getter
homekit_value_t lamp_on_get() { 
    LOG("Return lamp on: %s", boolToString(_lamp_on));
    return HOMEKIT_BOOL(_lamp_on); 
}
// Setter
void lamp_on_set(homekit_value_t value) {
    LOG("");
    if (value.format != homekit_format_bool) {
        LOG("Invalid LAMP on-value format: %d", value.format);
        return;
    }
    LOG("Disarm LAMP timer");
    sdk_os_timer_disarm(&lamp_timer);
    LOG("LAPM on set: %s", boolToString(value.bool_value));
    lamp_state_set(value.bool_value);
}
// Characteristic
homekit_characteristic_t lamp_on = HOMEKIT_CHARACTERISTIC_(
    ON, false, .getter=lamp_on_get, .setter=lamp_on_set
);
// Identify
static void lamp_identify(homekit_value_t _value) {
    LOG("Lamp identify");
}
// Lamp helpers
void lamp_state_notify_homekit() {
    LOG("");
    homekit_value_t new_value = lamp_on_get();
    LOG("!> Notifying homekit LAMP state: '%s': [%s]", boolToString(_lamp_on), lamp_on.description);
    homekit_characteristic_notify(&lamp_on, new_value);
}

void lamp_state_set(bool on) {
    LOG("LAMP new value: %s, old value: %s", boolToString(on), boolToString(_lamp_on));
    if (_lamp_on != on) {
        _lamp_on = on;
        lamp_write(_lamp_on);
        lamp_state_notify_homekit();
    }
}

static void lamp_timer_callback(void *arg) {
    LOG("Lamp timer fired");
    sdk_os_timer_disarm(&lamp_timer);
    lamp_state_set(false);
}

static void init_lamp_timer() {
    LOG("Initialize delayed turn off lamp timer");
    sdk_os_timer_disarm(&lamp_timer);
    sdk_os_timer_setfn(&lamp_timer, lamp_timer_callback, NULL);
}

int _lamp_turned_on_from_interrupt;

void lamp_delayed_off_observer_task(void *pvParameters) {
    _lamp_turned_on_from_interrupt = 0;
    while (1) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        if (_lamp_turned_on_from_interrupt != 0 && _lamp_on) {
            LOG("Arm Lamp timer");
            _lamp_turned_on_from_interrupt = 0;
            sdk_os_timer_arm(&lamp_timer, LAMP_ON_DURATION * 1000, false);
        }
    }
}

void lamp_delayed_off_observer() {
    LOG("");
    xTaskCreate(lamp_delayed_off_observer_task, "Lamp delayed off", 512, NULL, 1, NULL);
}

//// Garage door opener ///////////////////////////////////////////////////////////
uint8_t _current_door_state = HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_UNKNOWN;
ETSTimer gdo_update_timer; // used for delayed updating from contact sensor
void current_door_state_update_from_sensor();
void current_door_state_set(uint8_t new_state);

// Current door state //
// Getter
homekit_value_t gdo_current_door_state_get() {
    if (_current_door_state == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_UNKNOWN) {
	    current_door_state_update_from_sensor();
    }
    LOG("Returning Current door state '%s'.", state_description(_current_door_state));
    return HOMEKIT_UINT8(_current_door_state);
}
// Characteristic
homekit_characteristic_t current_door_state = HOMEKIT_CHARACTERISTIC_(
    CURRENT_DOOR_STATE, HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED,
    .getter=gdo_current_door_state_get,
    .setter=NULL
);
// Target door state //
// Getter
homekit_value_t gdo_target_door_state_get() {
    uint8_t result = gdo_current_door_state_get().int_value;
    if (result == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING) {
	    result = HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_OPEN;
    } else if (result == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING) {
        result = HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED;  
    }
    LOG("Returning Target door state '%s'.", state_description(result));
    return HOMEKIT_UINT8(result);
}
// Setter
void gdo_target_door_state_set(homekit_value_t new_value) {
    LOG("");
    if (new_value.format != homekit_format_uint8) {
        LOG("Invalid value format: %d", new_value.format);
        return;
    }
    if (_current_door_state != HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN &&
        _current_door_state != HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED) {
        LOG("gdo_target_door_state_set() ignored: current state not open or closed (%s).", state_description(_current_door_state));
	    return;
    }
    if (_current_door_state == new_value.int_value) {
        LOG("gdo_target_door_state_set() ignored: new target state == current state (%s).", state_description(_current_door_state));
        return;
    }
    // Toggle the garage door by toggling the relay connected to the GPIO (on - off):
    relay_write(true);
    vTaskDelay(RELAY_MS_DURATION / portTICK_PERIOD_MS);
    relay_write(false);

    current_door_state_set(
        (_current_door_state == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED)
        ? HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING
        : HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING
    );
    // Wait for the garage door to open / close,
    // then update current_door_state from sensor:
    LOG("Arm GDO timer");
    sdk_os_timer_arm(&gdo_update_timer, OPEN_CLOSE_DURATION * 1000, false);
}
// Characteristic
homekit_characteristic_t target_door_state = HOMEKIT_CHARACTERISTIC_(
    TARGET_DOOR_STATE, HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED,
    .getter=gdo_target_door_state_get,
    .setter=gdo_target_door_state_set
);
// Identify
static void identify_gdo(homekit_value_t _value) {
    LOG("GDO identify");
}
// Garage Door helpers //
void gdo_current_door_state_notify_homekit() {
    LOG("");
    homekit_value_t new_value = HOMEKIT_UINT8(_current_door_state);
    LOG("!> Notifying homekit CURRENT DOOR state: '%s' [%s]", state_description(_current_door_state), current_door_state.description);
    homekit_characteristic_notify(&current_door_state, new_value);
}

void gdo_target_door_state_notify_homekit() {
    LOG("");
    homekit_value_t new_value = gdo_target_door_state_get();
    LOG("!> Notifying homekit TARGET DOOR state: '%s' [%s]", state_description(new_value.int_value), target_door_state.description);
    homekit_characteristic_notify(&target_door_state, new_value);
}
void current_door_state_set(uint8_t new_state) {
    LOG("New state: %d, old state: %d", new_state, _current_door_state);
    if (_current_door_state != new_state) {
        _current_door_state = new_state;
        gdo_target_door_state_notify_homekit();
        gdo_current_door_state_notify_homekit();
    }
}

void current_door_state_update_from_sensor() {
    bool sensor_state = contact_sensor_state_get(REED_PIN);
    LOG("Sensor state: %s", boolToString(sensor_state));

    if (sensor_state) {
        current_door_state_set(HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN);
    } else {
        current_door_state_set(HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED);
    }
}

/**
 * Called (indirectly) from the interrupt handler to notify the client of a state change.
 **/
void contact_sensor_state_changed(uint8_t gpio_num, bool gpio_state) {
    LOG("REED SWITCH Interrupt GPIO%d: %s", gpio_num, boolToString(gpio_state));
    
    // Turn on LED when door open
    led_write(gpio_state);
    
    // Turn on light when door open
    lamp_state_set(gpio_state);
    _lamp_turned_on_from_interrupt += 1;

    if (_current_door_state == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING ||
        _current_door_state == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING) {
	    // Ignore the event - the state will be updated after the time expired!
        LOG("ignored during opening or closing.");
	    return;
    }
    current_door_state_update_from_sensor();
}

static void gdo_timer_callback(void *arg) {
    LOG("Timer fired. Updating state from sensor.");
    sdk_os_timer_disarm(&gdo_update_timer);
    current_door_state_update_from_sensor();
}

static void init_gdo_timer() {
    LOG("Initialize garage door opener timer");
    sdk_os_timer_disarm(&gdo_update_timer);
    sdk_os_timer_setfn(&gdo_update_timer, gdo_timer_callback, NULL);
}

//// Accessory definition
homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1, 
        .category=homekit_accessory_category_garage, 
        .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(
                ACCESSORY_INFORMATION, 
                .characteristics=(homekit_characteristic_t*[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, "Garage Door"),
                    HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Plum"),
                    HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "237A2BAB119E"),
                    HOMEKIT_CHARACTERISTIC(MODEL, "PlumDoorOpener"),
                    HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify_gdo),
                    NULL
                }
            ),
            HOMEKIT_SERVICE(
                GARAGE_DOOR_OPENER, 
                .primary=true, 
                .characteristics=(homekit_characteristic_t*[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, "Garage Door"),
                    &current_door_state,
                    &target_door_state,
                    HOMEKIT_CHARACTERISTIC(OBSTRUCTION_DETECTED, false),
                    NULL
                }
            ),
            NULL
        }
    ),
    HOMEKIT_ACCESSORY(
        .id=2, 
        .category=homekit_accessory_category_lightbulb, 
        .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
                HOMEKIT_CHARACTERISTIC(NAME, "Garage Lamp"),
                HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Plum"),
                HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "037A2BABF19D"),
                HOMEKIT_CHARACTERISTIC(MODEL, "PlumLamp"),
                HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
                HOMEKIT_CHARACTERISTIC(IDENTIFY, lamp_identify),
                NULL
            }),
            HOMEKIT_SERVICE(LIGHTBULB, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
                HOMEKIT_CHARACTERISTIC(NAME, "Garage Lamp"),
                &lamp_on,
                NULL
            }),
            NULL
        }
    ),
    NULL
};

static homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-987"
};

//// Main ////////////////////////////////////////////////

void logVersion() {
    LOG("Garage Door - init - v1 | %s", BUILD_DATETIME);
}

void start_sntp() {
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

void on_ping_watchdog_fail_callback() {
    LOG("Ping watchdog failed 30 times. Restart the device");
    sdk_system_restart();
}

void on_wifi_ready() {
    LOG("");
    start_sntp();
    logVersion();
    LOG("Starting TFTP server...");
    ota_tftp_init_server(TFTP_PORT);
    homekit_server_init(&config);
    ip_addr_t to_ping = get_gw_ip();
    start_ping_watchdog(to_ping, 30, 30, on_ping_watchdog_fail_callback);
}

void wifi_task(void *_args) {
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

static void wifi_init() {
    LOG("Start WiFi | SSID: %s | Password: %s", WIFI_SSID, WIFI_PASSWORD);
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };
    sysparam_set_string("hostname", "esp8266x1");
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
    
    xTaskCreate(wifi_task, "WiFi task", 1024, NULL, 1, NULL);
}

static void ota_config() {
    rboot_config conf = rboot_get_config();
    printf("\r\n\r\nOTA Basic demo.\r\nCurrently running on flash slot %d / %d.\r\n\r\n",
           conf.current_rom, conf.count);

    printf("Image addresses in flash:\r\n");
    for(int i = 0; i <conf.count; i++) {
        printf("%c%d: offset 0x%08x\r\n", i == conf.current_rom ? '*':' ', i, conf.roms[i]);
    }
}

void user_init(void) {
    #ifdef GARAGE_DEBUG_UDP
    udplog_init(3);
    #endif /* GARAGE_DEBUG_UDP */
    
    uart_set_baud(0, 115200);

    ota_config();
    
    LOG("START");
    
    logVersion();

    gpio_init();

    init_lamp_timer();
    init_gdo_timer();

    LOG("Using REED Switch at GPIO%d.", REED_PIN);
    if (interrupt_gpio_create(REED_PIN, false, false, 500, contact_sensor_state_changed)) {
        LOG("Failed to initialize door");
    }

    lamp_delayed_off_observer();
    
    wifi_init();
}