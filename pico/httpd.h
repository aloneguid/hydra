#pragma once
#include "pico/stdlib.h"
#include <string>
#include <functional>
#include "model.h"
#include "websocket.h"

class httpd {
public:
    static httpd* g_httpd;
    bool is_connected{false};
    int connection_attempts{0};
    std::string ip4addr;
    absolute_time_t start_time;
    app_state& as;
    ws_server ws;

    httpd(app_state& as) : as(as) {}

    void init();
    void connect();
    void start();

    // Push current state to the connected WebSocket client.
    // Must be called from within the lwIP context (TCP callback or
    // between cyw43_arch_lwip_begin() / cyw43_arch_lwip_end()).
    void notify();

    // commands
    std::function<void(const uint8_t report[8])> cmd_kbd_report;  // 8-byte HID keyboard report
    std::function<void(const uint8_t report[4])> cmd_mouse_report;  // 4-byte HID mouse report
    std::function<void()> cmd_reboot;
    std::function<void()> cmd_bt_adv_toggle;
    std::function<void(uint16_t central_id)> cmd_bt_central_activate;
    std::function<void(uint16_t central_id)> cmd_bt_central_unpair;
    std::function<void(const std::string& text)> cmd_type;

private:
    void update_as_cache();
};