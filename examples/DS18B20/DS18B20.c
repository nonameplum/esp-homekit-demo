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
#include <debug_helper.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#include "wifi.h"
#include <interrupt_gpio.h>
#include <spi_ota_build_failure.h>
#include <wifi_setup.h>
#include <udp_command.h>

#include "ds18b20/ds18b20.h"

#ifndef DATA_PIN
#error "DATA_PIN (GPIO) for DS18B20 must be defined"
#define DATA_PIN
#endif

// HOMEKIT
void temperature_sensor_identify(homekit_value_t _value) {
    LOG("Temperature sensor identify");
}

homekit_characteristic_t temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "TemperatureSensor"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Plum"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0012345"),
            HOMEKIT_CHARACTERISTIC(MODEL, "PLTempSensor"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, temperature_sensor_identify),
            NULL
        }),
        HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Temperature Sensor"),
            &temperature,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-987",
};

// GPIO
static void gpio_init() {
    gpio_set_pullup(DATA_PIN, false, false); 
}

// DS18B20
static void temperature_task(void *pvParameters) {
    uint8_t amount = 0;
    uint8_t sensors = 1;
    ds18b20_addr_t addrs[sensors];
    float results[sensors];
    char msg[100];
    LOG("Start temperature measure");

    while (1) {
        amount = ds18b20_scan_devices(DATA_PIN, addrs, sensors);

        if (amount < sensors) {
            LOG("Something is wrong, I expect to see %d sensors but just %d was detected!", sensors, amount);
        }

        ds18b20_measure_and_read_multi(DATA_PIN, addrs, sensors, results);
        for (int i = 0; i < sensors; ++i) {
            // ("\xC2\xB0" is the degree character (U+00B0) in UTF-8)
            sprintf(msg, "Sensor %08x%08x reports: %f \xC2\xB0""C", (uint32_t)(addrs[i] >> 32), (uint32_t)addrs[i], results[i]);
            LOG("%s", msg);
            temperature.value.float_value = results[i];
            homekit_characteristic_notify(&temperature, HOMEKIT_FLOAT(results[i]));
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

static void temperature_task_init() {
    xTaskCreate(&temperature_task, "temperature_task", 512, NULL, 1, NULL);
}

// UDP commands
void udp_command_read_temp(char *result, int result_size, char *param) {
    snprintf(result, result_size, "%.2f \xC2\xB0""C", temperature.value.float_value);
}

// Main
static void wifi_connected_handler() {
    LOG("Homekit server init");
    homekit_server_init(&config);
    udp_command_server_task_start_with_default_commands(9876);
    udp_command_add("read_temp", udp_command_read_temp);
}

void user_init(void) {
    #ifdef DEBUG_HELPER_UDP
    udplog_init(3);
    #endif /* DEBUG_HELPER_UDP */
    
    uart_set_baud(0, 115200);

    init_ota_update_failure_check(BUILD_DATETIME);

    gpio_init();
    temperature_task_init();

    wifi_init(WIFI_SSID, WIFI_PASSWORD, "iot_temp", true, wifi_connected_handler);
}