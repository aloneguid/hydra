#pragma once
#include <string>
#include <vector>

struct app_bt_central {
    uint16_t id;
    bool is_active;
    std::string addr;
    std::string addr_type;
};

struct app_state {
    bool is_advertising{false};
    int bt_central_count{0};
    std::vector<app_bt_central> bt_centrals;
    std::string bt_centrals_json_array;
};