#pragma once
#include "pico/stdlib.h"
#include <string>
#include <functional>

class httpd {
public:
    static httpd* g_httpd;
    bool is_connected{false};
    int connection_attempts{0};
    std::string ip4addr;
    absolute_time_t start_time;

    void init();
    void connect();
    void start();

private:
    
};