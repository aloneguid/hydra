#pragma once
#include "pico/stdlib.h"
#include <string>
#include <functional>
#include "model.h"

class httpd {
public:
    static httpd* g_httpd;
    bool is_connected{false};
    int connection_attempts{0};
    std::string ip4addr;
    absolute_time_t start_time;
    app_state& as;

    httpd(app_state& as) : as(as) {}

    void init();
    void connect();
    void start();

    // utils
    void update_as_cache();

    // commands
    std::function<void()> cmd_reboot;
    std::function<void()> cmd_bt_adv_toggle;
    std::function<void(uint16_t central_id)> cmd_bt_central_activate;
    std::function<void(const std::string& text)> cmd_type;

private:
};