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
#include <time.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <udplogger.h>

#include "wifi.h"
#include <interrupt_gpio.h>
#include <debug_helper.h>
#include <spi_ota_build_failure.h>
#include <wifi_setup.h>

ETSTimer lamp_timer;
ETSTimer await_door_close_timer;
homekit_value_t light_on_get();
homekit_characteristic_t light_sensor_characteristic;

// GPIO 
void led_write(bool on) {
    LOG("GPIO%d (LED): %s", LED_PIN, boolToString(on));
    gpio_write(LED_PIN, !on);
}

bool relay_read() {
    bool on = gpio_read(RELAY_PIN);
    LOG("GPIO%d (RELAY): %s", RELAY_PIN, boolToString(on));
    return on;
}

void relay_write(bool on) {
    bool prevOn = relay_read();
    LOG("GPIO%d (RELAY): %s, prevOn: %s", RELAY_PIN, boolToString(on), boolToString(prevOn));
    if (on != prevOn) {
        gpio_write(RELAY_PIN, on);
        homekit_characteristic_notify(&light_sensor_characteristic, light_on_get());
    }
}

void gpio_init() {
    gpio_enable(LED_PIN, GPIO_OUTPUT);
    gpio_enable(RELAY_PIN, GPIO_OUTPUT);
    gpio_write(RELAY_PIN, false);
}

//// START: FSM ///////////////////////////////////
enum State {
    ST_OFF,
    ST_ON,
    ST_ON_AUTO_OFF,
    ST_LEAVING_HOME,
    ST_MAX
};

enum Event {
    EV_SWITCH,
    EV_GARAGE_OPENED,
    EV_GARAGE_OPENED_CONDITIONAL,
    EV_DELAYED_OFF,
    EV_AWAIT_DOOR_CLOSE_FINISHED,
    EV_GARAGE_CLOSED,
    EV_MAX
};

void step_state(enum Event event);

// TIMERS
void stop_timers() {
    sdk_os_timer_disarm(&lamp_timer);
    sdk_os_timer_disarm(&await_door_close_timer);
}

// - LAMP TIMER
void start_lamp_timer() {
    LOG("Start lamp timer: 3 minutes");
    sdk_os_timer_arm(&lamp_timer, 3 * 60 * 1000, false);
}

static void lamp_timer_callback(void *arg) {
    LOG("Lamp timer fired");
    sdk_os_timer_disarm(&lamp_timer);
    step_state(EV_DELAYED_OFF);
}

static void init_lamp_timer() {
    LOG("Initialize delayed turn off lamp timer");
    sdk_os_timer_disarm(&lamp_timer);
    sdk_os_timer_setfn(&lamp_timer, lamp_timer_callback, NULL);
}

// - AWAIT DOOR CLOSE TIMER
void start_await_door_close_timer() {
    LOG("Start await door close timer: 3 minutes");
    sdk_os_timer_arm(&await_door_close_timer, 3 * 60 * 1000, false);
}

static void await_door_close_timer_callback(void *arg) {
    LOG("Await door close timer fired");
    sdk_os_timer_disarm(&await_door_close_timer);
    step_state(EV_AWAIT_DOOR_CLOSE_FINISHED);
}

static void init_await_door_close_timer() {
    LOG("Initialize awaiting door close timer");
    sdk_os_timer_disarm(&await_door_close_timer);
    sdk_os_timer_setfn(&await_door_close_timer, await_door_close_timer_callback, NULL);
}

// FSM Variables
enum State state = ST_OFF;
#define AWAIT_GARAGE_DOOR_CLOSE_TIME_MS 3 * 60 * 1000
TickType_t lastLightOnTickCount = 0;

// Helpers
const char *state_description(enum State state) {
    switch (state) {
        case ST_OFF: return "ST_OFF";
        case ST_ON: return "ST_ON";
        case ST_ON_AUTO_OFF: return "ST_ON_AUTO_OFF";
        case ST_LEAVING_HOME: return "ST_LEAVING_HOME";
        default: return "unknown";
    }
}

const char *event_description(enum Event event) {
    switch (event) {
        case EV_SWITCH: return "EV_SWITCH";
        case EV_GARAGE_OPENED: return "EV_GARAGE_OPENED";
        case EV_GARAGE_OPENED_CONDITIONAL: return "EV_GARAGE_OPENED_CONDITIONAL";
        case EV_DELAYED_OFF: return "EV_DELAYED_OFF";
        case EV_AWAIT_DOOR_CLOSE_FINISHED: return "EV_AWAIT_DOOR_CLOSE_FINISHED";
        case EV_GARAGE_CLOSED: return "EV_GARAGE_CLOSED";
        default: return "unknown";
    }
}

void LOG_STATE_EVENT(const char * const name, enum State state, enum Event event) {
    LOG("%s | state: %s | event: %s", name, state_description(state), event_description(event));
}

// State handlers
enum State light_off(enum State state, enum Event event) {
    LOG_STATE_EVENT("light_off", state, event);
    stop_timers();
    relay_write(false);
    return ST_OFF;
}

enum State light_on(enum State state, enum Event event) {
    LOG_STATE_EVENT("light_on", state, event);
    stop_timers();
    relay_write(true);
    lastLightOnTickCount = xTaskGetTickCount();
    return ST_ON;
}

enum State light_on_auto_off(enum State state, enum Event event) {
    LOG_STATE_EVENT("light_on_auto_off", state, event);
    stop_timers();
    start_lamp_timer();
    relay_write(true);
    return ST_ON_AUTO_OFF;
}

enum State light_on_conditional(enum State state, enum Event event) {
    LOG_STATE_EVENT("light_on_conditional", state, event);
    stop_timers();
    relay_write(true);
    TickType_t currentLightOnTickCount = xTaskGetTickCount();
    TickType_t passedMs = (currentLightOnTickCount - lastLightOnTickCount) * 10;
    if (passedMs <= AWAIT_GARAGE_DOOR_CLOSE_TIME_MS) {
        start_await_door_close_timer();
        return ST_LEAVING_HOME;
    }
    start_lamp_timer();
    return ST_ON_AUTO_OFF;
}

// State transitions table
typedef enum State (*event_handler)(enum State, enum Event);

event_handler transitions[ST_MAX][EV_MAX] = {
    [ST_OFF] = {   
        [EV_SWITCH] = light_on,
        [EV_GARAGE_OPENED] = light_on_auto_off, 
    },
    [ST_ON] = {   
        [EV_SWITCH] = light_off,
        [EV_GARAGE_OPENED_CONDITIONAL] = light_on_conditional,
    },
    [ST_ON_AUTO_OFF] = {
        [EV_SWITCH] = light_off,
        [EV_DELAYED_OFF] = light_off, 
    },
    [ST_LEAVING_HOME] = {
        [EV_AWAIT_DOOR_CLOSE_FINISHED] = light_off,
        [EV_GARAGE_CLOSED] = light_off, 
        [EV_SWITCH] = light_off,
    },
};

void step_state(enum Event event) {
    LOG_STATE_EVENT("!>>> step_state: received event", state, event);
    event_handler handler = transitions[state][event];
    if (!handler) {
        LOG("No state handle for state: %d, event: %d", state, event);
        return;
    }
    state = handler(state, event);
    LOG_STATE_EVENT("!<<< step_state: new state", state, event);
}

//// END: FSM ///////////////////////////////////

// Input pin callback
void input_callback(uint8_t gpio_num, bool gpio_state) {
    LOG("Interrupt GPIO%d [Outside LAMP]: %s", gpio_num, boolToString(gpio_state));
    switch (gpio_num) {
        case INPUT_PIN:
            if (gpio_state) {
                step_state(EV_GARAGE_OPENED);
            } else {
                step_state(EV_GARAGE_CLOSED);
            }
            break;
        case SWITCH_PIN:
            step_state(EV_SWITCH);
            break;
    }
}

// HomeKit
// Identify
void light_sensor_identify(homekit_value_t _value) {
    LOG("Light sensor identify");
}

void light_identify(homekit_value_t _value) {
    LOG("Light identify");
}

// Getter & Setter
homekit_value_t light_on_get() {
    return HOMEKIT_BOOL(relay_read());
}

void light_on_set(homekit_value_t value) {
    step_state(EV_SWITCH);
}

// Characteristics
homekit_characteristic_t light_sensor_characteristic = HOMEKIT_CHARACTERISTIC_(CONTACT_SENSOR_STATE, 0,
    .getter=light_on_get,
    .setter=NULL,
    NULL
);

// Accessories
homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1, 
        .category=homekit_accessory_category_lightbulb, 
        .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
                HOMEKIT_CHARACTERISTIC(NAME, "GarageInsideLight"),
                HOMEKIT_CHARACTERISTIC(MANUFACTURER, "PLUM"),
                HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "037A2BABF19D"),
                HOMEKIT_CHARACTERISTIC(MODEL, "Light"),
                HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
                HOMEKIT_CHARACTERISTIC(IDENTIFY, light_identify),
                NULL
            }),
            HOMEKIT_SERVICE(LIGHTBULB, .primary=true, .characteristics=(homekit_characteristic_t*[]){
                HOMEKIT_CHARACTERISTIC(NAME, "GarageInsideLight"),
                HOMEKIT_CHARACTERISTIC(
                    ON, false,
                    .getter=light_on_get,
                    .setter=light_on_set
                ),
                NULL
            }),
            NULL
        }
    ),
    HOMEKIT_ACCESSORY(
        .id=2,
        .category=homekit_accessory_category_sensor,
        .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(
                ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, "LightSensor"),
                    HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Plum"),
                    HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "2012345"),
                    HOMEKIT_CHARACTERISTIC(MODEL, "LSx1"),
                    HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, light_sensor_identify),
                    NULL
                },
            ),
            HOMEKIT_SERVICE(
                CONTACT_SENSOR,
                .primary=true,
                .characteristics=(homekit_characteristic_t*[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, "Contact"),
                    &light_sensor_characteristic,
                    NULL
                },
            ),
            NULL
        },
    ),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-987"
};

// Main
void wifi_connected_handler() {
    homekit_server_init(&config);
}

void user_init(void) {
    #ifdef GARAGE_DEBUG_UDP
    udplog_init(3);
    #endif /* GARAGE_DEBUG_UDP */
    
    uart_set_baud(0, 115200);
    
    LOG("START");

    init_ota_update_failure_check(BUILD_DATETIME, 10, 60 * 1000);

    gpio_init();
    led_write(false);
    init_lamp_timer();
    init_await_door_close_timer();
    
    LOG("Create input interrupt on INPUT_PIN [GPIO%d]", INPUT_PIN);
    interrupt_gpio_create(INPUT_PIN, true, true, 500, input_callback);

    LOG("Create input interrupt on SWITCH_PIN [GPIO%d]", SWITCH_PIN);
    interrupt_gpio_create(SWITCH_PIN, true, true, 500, input_callback);

    wifi_init(WIFI_SSID, WIFI_PASSWORD, "esp8266xg2", true, wifi_connected_handler);
}