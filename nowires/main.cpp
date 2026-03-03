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
    bool is_connected{false};

    void init() {
        // Enable Wi-Fi station mode (regular Wi-Fi client)
        cyw43_arch_enable_sta_mode();
    }

    void connect() {
        // Connect to Wi-Fi network
        log("Connecting to Wi-Fi network: %s\n", WIFI_SSID);
        if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
            log("Failed to connect to Wi-Fi\n");
            return;
        }
        log("Connected to Wi-Fi\n");
        is_connected = true;
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
    h.connect();

    if(!h.is_connected) {
        log("Not connected to Wi-Fi, exiting\n");
        return -1;
    }

    while (true) {
        log("Hello, world! %s:%s\n", WIFI_SSID, WIFI_PASSWORD);

        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(500);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(500);
    }
}
