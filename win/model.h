#pragma once

namespace hydra {
    struct key_event {
        uint8_t vk_code;
        bool down;  // true - down, false - up
    };

    enum class mouse_button {
        none = 0,
        left,
        right,
        middle,
        x
    };

    struct mouse_event {
        long x;
        long y;
        mouse_button button;
        bool down;  // true - down, false - up
        short wheel_delta;
    };
}