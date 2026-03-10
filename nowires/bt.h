#pragma once
#include "pico/stdlib.h"
#include "model.h"

// btstack
#include "btstack.h"
#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"
#include "ble/gatt-service/hids_device.h"
#include "device.h" // generated from .gatt by GATT compiler
#include "hid.h"

class bt {
public:
    uint8_t battery = 95;
    app_state& as;
    static bt* g_bt;

    bt(app_state& as);

    void init();
    void start();

    bool adv() const {
        return is_advertising;
    }
    void adv_toggle();
    bool activate_central(uint16_t central_id);
    void unpair_central(uint16_t central_id);

    void update_as();

    // HID utils
    void send_key_press(uint8_t keycode);

private:
    bool is_advertising{false};

};
