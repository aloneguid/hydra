#include "dashboard.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"

void led_put(bool on) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

void led_blink(int times, int delay_ms) {
    for (int i = 0; i < times; i++) {
        led_put(true);
        sleep_ms(delay_ms);
        led_put(false);
        sleep_ms(delay_ms);
    }
}

void led2_blink() {
    // blink LED on GPIO 6
    cyw43_arch_gpio_put(6, true);
    sleep_ms(1000);
    cyw43_arch_gpio_put(6, false);
}
