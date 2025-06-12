#include "dashboard.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"

// declare static variables
uint32_t dashboard::mouse_hid_reports_sent = 0;
uint32_t dashboard::mouse_hid_buffers_full = 0;
uint32_t dashboard::keyboard_hid_reports_sent = 0;
uint32_t dashboard::keyboard_hid_buffers_full = 0;

void dashboard::led_put(bool on) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

void dashboard::led_blink(int times, int delay_ms) {
    for (int i = 0; i < times; i++) {
        led_put(true);
        sleep_ms(delay_ms);
        led_put(false);
        sleep_ms(delay_ms);
    }
}

void dashboard::print_stats() {
    printf("\t\tKEYBOARD\tMOUSE\n");
    printf("SENT\t\t%u\t\t%u\n", keyboard_hid_reports_sent, mouse_hid_reports_sent);
    printf("OVERFLOW\t%u\t\t%u\n", dashboard::keyboard_hid_buffers_full, dashboard::mouse_hid_buffers_full);
}

void dashboard::reset_stats() {
    mouse_hid_reports_sent = 0;
    mouse_hid_buffers_full = 0;
    keyboard_hid_reports_sent = 0;
    keyboard_hid_buffers_full = 0;
}
