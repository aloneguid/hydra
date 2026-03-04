#include <stdio.h>
#include <string>
#include "log.h"
#include "httpd.h"
#include "bt.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

using namespace std;

app_state as;

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

void led_blink_sos_forever() {
    while (true) {
        // S
        led_put(true);
        sleep_ms(200);
        led_put(false);
        sleep_ms(200);
        led_put(true);
        sleep_ms(200);
        led_put(false);
        sleep_ms(200);
        led_put(true);
        sleep_ms(200);
        led_put(false);
        sleep_ms(600);

        // O
        led_put(true);
        sleep_ms(600);
        led_put(false);
        sleep_ms(200);
        led_put(true);
        sleep_ms(600);
        led_put(false);
        sleep_ms(200);
        led_put(true);
        sleep_ms(600);
        led_put(false);
        sleep_ms(600);

        // S
        led_put(true);
        sleep_ms(200);
        led_put(false);
        sleep_ms(200);
        led_put(true);
        sleep_ms(200);
        led_put(false);
        sleep_ms(200);
        led_put(true);
        sleep_ms(200);
        led_put(false);
        sleep_ms(1000); // wait before repeating
    }
}

int main() {
    // start_time = get_absolute_time();
    stdio_init_all();

    log("");
    log("-----");
    log("Hydra");

    // Initialise the Wi-Fi/bluetooth chip
    if (cyw43_arch_init()) {
        log("Wireless chip init failed\n");
        led_blink_sos_forever();
    }

    led_blink(5, 100);  // indicate that the chip was initialized successfully

    // http startup
    httpd h{as};
    h.init();
    while(!h.is_connected) {
        h.connect();

        if(!h.is_connected) {
            led_blink(3, 500);
        }
    }
    h.start();

    // bt startup
    bt b{as};
    b.init();
    b.start();

    h.cmd_bt_adv_toggle = [&b]() {
        b.adv_toggle();
    };

    while (true) {

#if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(led_time);
#else
        // sleep_ms(1000);
        led_blink(1, 2000);
#endif

        // log("Hydra [%s] %s:%s", VTAG, WIFI_SSID, WIFI_PASSWORD);

        // cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        // sleep_ms(500);
        // cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        // sleep_ms(500);
    }
}
