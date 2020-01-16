#include <string.h>
#include <etstimer.h>
#include <esplibs/libmain.h>
#include "interrupt_gpio.h"
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "esp8266.h"

typedef struct _interrupt_gpio {
    bool inverted;
    uint8_t gpio_num;
    uint16_t debounce_time;
    uint32_t last_event_time;
    interrupt_gpio_callback_fn callback;
    QueueHandle_t queue_handle;
    TaskHandle_t task_handle;
    struct _interrupt_gpio *next;
} interrupt_gpio_t;

interrupt_gpio_t *gpios = NULL;

static interrupt_gpio_t *interrupt_gpio_find_by_gpio(const uint8_t gpio_num) {
    interrupt_gpio_t *gpio_intr = gpios;
    while (gpio_intr && gpio_intr->gpio_num != gpio_num)
        gpio_intr = gpio_intr->next;

    return gpio_intr;
}

bool interrupt_gpio_state_get(uint8_t gpio_num) {
    interrupt_gpio_t *gpio_intr = interrupt_gpio_find_by_gpio(gpio_num);
    return gpio_read(gpio_num) ^ gpio_intr->inverted;
}

void interrupt_gpio_intr_callback(uint8_t gpio_num) {
    interrupt_gpio_t *gpio_intr = interrupt_gpio_find_by_gpio(gpio_num);
    if (!gpio_intr) {
        return;
    }
    uint32_t now = xTaskGetTickCountFromISR();
    xQueueSendToBackFromISR(gpio_intr->queue_handle, &now, NULL);
}

void gpio_interrupt_task(void *pvParameters) {
    interrupt_gpio_t *gpio_intr = (interrupt_gpio_t *)pvParameters;
    gpio_set_interrupt(gpio_intr->gpio_num, GPIO_INTTYPE_EDGE_ANY, interrupt_gpio_intr_callback);
    uint32_t last = 0;
    bool last_state = interrupt_gpio_state_get(gpio_intr->gpio_num);
    while (1) {
        uint32_t gpio_ts;
        xQueueReceive(gpio_intr->queue_handle, &gpio_ts, portMAX_DELAY);
        gpio_ts *= portTICK_PERIOD_MS;
        bool state = interrupt_gpio_state_get(gpio_intr->gpio_num);
        if ((state != last_state) && (last < gpio_ts - gpio_intr->debounce_time)) {
            gpio_intr->callback(gpio_intr->gpio_num, state);
            last = gpio_ts;
            last_state = state;
        }
    }
}

int interrupt_gpio_create(
    const uint8_t gpio_num, 
    const bool pullup, 
    const bool inverted, 
    const uint16_t debounce_time, 
    interrupt_gpio_callback_fn callback) 
{
    interrupt_gpio_t *gpio = interrupt_gpio_find_by_gpio(gpio_num);
    if (gpio) {
        return -1;
    }

    gpio = malloc(sizeof(interrupt_gpio_t));
    memset(gpio, 0, sizeof(*gpio));

    gpio->inverted = inverted;
    gpio->gpio_num = gpio_num;
    gpio->callback = callback;
    // times in milliseconds
    gpio->debounce_time = debounce_time;
    uint32_t now = xTaskGetTickCountFromISR();
    gpio->last_event_time = now;
    gpio->queue_handle = xQueueCreate(2, sizeof(uint32_t));
    gpio->task_handle = NULL;
    gpio->next = gpios;
    gpios = gpio;

    gpio_enable(gpio_num, GPIO_INPUT);
    gpio_set_pullup(gpio_num, pullup, false);
    
    char task_name [16];
    sprintf(task_name, "gpiotask%d", gpio_num);
    xTaskCreate(gpio_interrupt_task, task_name, 256, (void *)gpio, 2, &gpio->task_handle);

    return 0;
}

void interrupt_gpio_delete(const uint8_t gpio_num) {
    if (!gpios)
        return;

    interrupt_gpio_t *gpio = NULL;
    if (gpio->gpio_num == gpio_num) {
        // Skip first element:
        gpio = gpios;
        gpios = gpios->next;
    } else {
        interrupt_gpio_t *b = gpios;
        while (b->next) {
            if (b->next->gpio_num == gpio_num) {
                // Unlink middle element:
                gpio = b->next;
                b->next = b->next->next;
                break;
            }
        }
    }

    if (gpio) {
        gpio_set_interrupt(gpio->gpio_num, GPIO_INTTYPE_EDGE_ANY, NULL);
        vTaskDelete(gpio->task_handle);
        vQueueDelete(gpio->queue_handle);
        free(gpio);
    }
}

