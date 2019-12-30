#pragma once

typedef void (*interrupt_gpio_callback_fn)(uint8_t gpio_num, bool event);

int interrupt_gpio_create(
    const uint8_t gpio_num, 
    const bool pullup, 
    const bool inverted, 
    const uint16_t debounce_time, 
    interrupt_gpio_callback_fn callback);
void interrupt_gpio_destroy(uint8_t gpio_num);
bool interrupt_gpio_state_get(uint8_t gpio_num);
