#pragma once
#include <cstdint>

class dashboard {
public:

    // statistics
    static uint32_t mouse_hid_reports_sent;
    static uint32_t mouse_hid_buffers_full;
    static uint32_t keyboard_hid_reports_sent;
    static uint32_t keyboard_hid_buffers_full;

    static void led_put(bool on);
    static void led_blink(int times, int delay_ms = 100);

    static void print_stats();
    static void reset_stats();
};