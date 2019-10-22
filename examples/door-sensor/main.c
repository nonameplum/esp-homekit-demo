#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"
#include "contact_sensor.h"

const int led_gpio = 13;
const int reed_gpio = 14;

void led_write(bool on) {
    printf("Led write: %d.\n", on ? 1 : 0);
    gpio_write(led_gpio, on ? 1 : 0);
}

static void wifi_init() {
    printf("Wifi init.\n");
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
}

/**
 * Called during pairing to let the user know which accessorty is being paired.
 **/
void door_identify(homekit_value_t _value) {
    printf("Door identifying\n");
    // The sensor cannot identify itself.
    // Nothing to do here.
}

/**
 * Returns the door sensor state as a homekit value.
 **/
homekit_value_t door_state_getter() {
    bool isOpen = contact_sensor_state_get(reed_gpio) == CONTACT_OPEN;
    printf("Door state was requested (%s).\n", isOpen ? "open" : "closed");
    led_write(isOpen);
    return HOMEKIT_UINT8(contact_sensor_state_get(reed_gpio) == CONTACT_OPEN ? 1 : 0);
}

/**
 * The sensor characteristic as global variable.
 **/
homekit_characteristic_t door_open_characteristic = HOMEKIT_CHARACTERISTIC_(CONTACT_SENSOR_STATE, 0,
    .getter=door_state_getter,
    .setter=NULL,
    NULL
);

/**
 * Called (indirectly) from the interrupt handler to notify the client of a state change.
 **/
void contact_sensor_callback(uint8_t gpio, contact_sensor_state_t state) {
    switch (state) {
        case CONTACT_OPEN:
        case CONTACT_CLOSED:
            printf("Pushing contact sensor state '%s'.\n", state == CONTACT_OPEN ? "open" : "closed");
            homekit_characteristic_notify(&door_open_characteristic, door_state_getter());
            break;
        default:
            printf("Unknown contact sensor event: %d\n", state);
    }
}

/**
 * An array of the accessories (one) provided contining one service.
 **/
homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1,
        .category=homekit_accessory_category_sensor,
        .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(
                ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, "Contact Sensor"),
                    HOMEKIT_CHARACTERISTIC(MANUFACTURER, "ObjP"),
                    HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "2012345"),
                    HOMEKIT_CHARACTERISTIC(MODEL, "DS1"),
                    HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, door_identify),
                    NULL
                },
            ),
            HOMEKIT_SERVICE(
                CONTACT_SENSOR,
                .primary=true,
                .characteristics=(homekit_characteristic_t*[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, "Kontakt"),
                    &door_open_characteristic,
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
    .password = "111-11-111"
};

void reed_callback(uint8_t gpio) {
    printf("reed_callback. Reed state: %d\n", gpio_read(reed_gpio));
    led_write(gpio_read(reed_gpio));
}

void user_init(void) {
    uart_set_baud(0, 115200);
    wifi_init();

    printf("Using Sensor at GPIO%d.\n", reed_gpio);
    if (contact_sensor_create(reed_gpio, contact_sensor_callback)) {
        printf("Failed to initialize door\n");
        led_write(false);
    }
    gpio_enable(led_gpio, GPIO_OUTPUT);

    homekit_server_init(&config);

    homekit_characteristic_notify(&door_open_characteristic, door_state_getter());
}

