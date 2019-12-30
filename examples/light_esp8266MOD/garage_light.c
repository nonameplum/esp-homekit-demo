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

#include "wifi.h"
#include <interrupt_gpio.h>
#include <debug_helper.h>

ETSTimer lamp_timer;

// GPIO 
void led_write(bool on) {
    LOG("GPIO%d (LED): %s", LED_PIN, boolToString(on));
    gpio_write(LED_PIN, !on);
}

void relay_write(bool on) {
    LOG("GPIO%d (RELAY): %s", RELAY_PIN, boolToString(on));
    gpio_write(RELAY_PIN, on);
}

void gpio_init() {
    gpio_enable(LED_PIN, GPIO_OUTPUT);
    gpio_enable(RELAY_PIN, GPIO_OUTPUT);
    gpio_write(RELAY_PIN, false);
}

//// FSM ///////////////////////////////////
enum State {
    ST_OFF,
    ST_ON,
    ST_TIMER_ON,
    ST_MAX
};

enum Event {
    EV_SWITCH,
    EV_INPUT_HEIGHT,
    EV_DELAYED_OFF,
    EV_MAX
};

const char *state_description(enum State state) {
    switch (state) {
        case ST_OFF: return "ST_OFF";
        case ST_ON: return "ST_ON";
        case ST_TIMER_ON: return "ST_TIMER_ON";
        default: return "unknown";
    }
}

const char *event_description(enum Event event) {
    switch (event) {
        case EV_SWITCH: return "EV_SWITCH";
        case EV_INPUT_HEIGHT: return "EV_INPUT_HEIGHT";
        case EV_DELAYED_OFF: return "EV_DELAYED_OFF";
        default: return "unknown";
    }
}

typedef enum State (*event_handler)(enum State, enum Event);

enum State state = ST_OFF;

void LOG_STATE_EVENT(const char * const name, enum State state, enum Event event) {
    LOG("%s | state: %s | event: %s", name, state_description(state), event_description(event));
}

void start_timer() {
    LOG("Start timer: 3 minutes");
    sdk_os_timer_arm(&lamp_timer, 3 * 60 * 1000, false);
}

void stop_timer() {
    LOG("Stop timer");
    sdk_os_timer_disarm(&lamp_timer);
}

enum State light_off(enum State state, enum Event event) {
    LOG_STATE_EVENT("light_off", state, event);
    stop_timer();
    relay_write(false);
    return ST_OFF;
}

enum State light_on(enum State state, enum Event event) {
    LOG_STATE_EVENT("light_on", state, event);
    stop_timer();
    relay_write(true);
    return ST_ON;
}

enum State start_delayed_off(enum State state, enum Event event) {
    LOG_STATE_EVENT("start_delayed_off", state, event);
    stop_timer();
    start_timer();
    relay_write(true);
    return ST_TIMER_ON;
}

event_handler transitions[EV_MAX][ST_MAX] = {
    [ST_OFF] = {   
        [EV_SWITCH] = light_on,
        [EV_INPUT_HEIGHT] = start_delayed_off, 
    },
    [ST_ON] = {   
        [EV_SWITCH] = light_off,
        [EV_INPUT_HEIGHT] = start_delayed_off, 
    },
    [ST_TIMER_ON] = {
        [EV_SWITCH] = light_off,
        [EV_DELAYED_OFF] = light_off, 
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
//// FSM ///////////////////////////////////

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

// Input pin callback
void input_callback(uint8_t gpio_num, bool gpio_state) {
    LOG("Interrupt GPIO%d: %s", gpio_num, boolToString(gpio_state));
    switch (gpio_num) {
        case INPUT_PIN:
            if (gpio_state) {
                step_state(EV_INPUT_HEIGHT);
            }
            break;
        case SWITCH_PIN:
            step_state(EV_SWITCH);
            break;
    }
}

// Main
void user_init(void) {
    #ifdef GARAGE_DEBUG_UDP
    udplog_init(3);
    #endif /* GARAGE_DEBUG_UDP */
    
    uart_set_baud(0, 115200);
    
    LOG("START");

    gpio_init();
    led_write(false);
    init_lamp_timer();
    
    LOG("Create input interrupt on INPUT_PIN [GPIO%d]", INPUT_PIN);
    interrupt_gpio_create(INPUT_PIN, true, true, 500, input_callback);

    LOG("Create input interrupt on SWITCH_PIN [GPIO%d]", SWITCH_PIN);
    interrupt_gpio_create(SWITCH_PIN, true, true, 500, input_callback);
}