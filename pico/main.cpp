#include <stdio.h>
#include <string>
#include "log.h"
#include "httpd.h"
#include "bt.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/watchdog.h"

using namespace std;

app_state as;

// --- ASCII to HID keycode conversion ---
static uint8_t ascii_to_hid(char c) {
    if (c >= 'a' && c <= 'z') return 0x04 + (c - 'a');  // a-z
    if (c >= 'A' && c <= 'Z') return 0x04 + (c - 'A');  // A-Z (same as lowercase, shift handled separately)
    if (c >= '1' && c <= '9') return 0x1E + (c - '1');  // 1-9
    if (c == '0') return 0x27;                           // 0
    if (c == ' ') return 0x2C;                           // Space
    if (c == '.') return 0x37;                           // .
    if (c == ',') return 0x36;                           // ,
    if (c == '\n') return 0x28;                          // Enter
    if (c == '\t') return 0x2B;                          // Tab
    if (c == '\r') return 0x28;                          // Enter
    return 0;  // unsupported character
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

    if (log_enabled()) {
        log("");
        log("-----");
        log("Hydra");
    }

    // Initialise the Wi-Fi/bluetooth chip
    if (cyw43_arch_init()) {
        if (log_enabled()) log("Wireless chip init failed\n");
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

    h.cmd_kbd_report = [&b](const uint8_t report[8]) {
        b.send_key_report(report);
    };

    h.cmd_mouse_report = [&b](const uint8_t report[4]) {
        b.send_mouse_report(report);
    };

    h.cmd_bt_adv_toggle = [&b]() {
        b.adv_toggle();
    };

    h.cmd_bt_central_activate = [&b](uint16_t central_id) {
        b.activate_central(central_id);
        b.update_as();
    };

    h.cmd_bt_central_unpair = [&b](uint16_t central_id) {
        b.unpair_central(central_id);
    };

    h.cmd_type = [&b](const string& text) {
        if (log_enabled()) log("Received text to type: %s", text.c_str());
        for (char c : text) {
            uint8_t hid_code = ascii_to_hid(c);
            if (hid_code != 0) {
                b.send_key_press(hid_code);
                sleep_ms(10);  // Small delay between keypresses
            }
        }
    };

    h.cmd_reboot = []() {
        if (log_enabled()) log("Rebooting...");
        sleep_ms(1000);
        watchdog_reboot(0, 0, 0);
    };

    // Timer for periodic state updates (uptime, BT name, etc.)
    absolute_time_t next_notify_time = get_absolute_time();
    const int NOTIFY_INTERVAL_MS = 5000;  // Send updates every 5 seconds

    while (true) {

#if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(led_time);
#else
        // sleep_ms(1000);
        led_blink(1, 2000);
#endif

        // Send periodic updates to WebSocket client
        absolute_time_t now = get_absolute_time();
        if (absolute_time_diff_us(next_notify_time, now) >= 0) {
            cyw43_arch_lwip_begin();
            h.notify();
            cyw43_arch_lwip_end();
            next_notify_time = delayed_by_ms(now, NOTIFY_INTERVAL_MS);
        }

        // log("Hydra [%s] %s:%s", VTAG, WIFI_SSID, WIFI_PASSWORD);

        // cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        // sleep_ms(500);
        // cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        // sleep_ms(500);
    }
}
