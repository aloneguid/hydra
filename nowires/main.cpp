#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

/**
 * I _prefer_ all of it in one single file.
 */

// flags
bool log_enabled = true;
// int line_id = 0;


// --- logging ---
void log(const char *format, ...) {
    if (!log_enabled) return;

    // convert line_id to string and print it, padding with spaces to 4 characters
    // printf("%d. ", line_id++);
    printf("log: ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

// --- dashboard ---

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

// --- http ---

class httpd {
public:
    void init() {
        // Enable Wi-Fi station mode (regular Wi-Fi client)
        cyw43_arch_enable_sta_mode();
    }
};

// --- main entry point ---

int main() {
    stdio_init_all();

    // Initialise the Wi-Fi/bluetooth chip
    if (cyw43_arch_init()) {
        log("Wireless chip init failed\n");
        return -1;
    }

    led_blink(5, 100);

    httpd h;
    h.init();

    while (true) {
        log("Hello, world!\n");

        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(500);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(500);
    }
}
