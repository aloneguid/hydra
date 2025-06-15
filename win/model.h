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
        // true - down, false - up
        bool down;
        short wheel_delta;
        // true if this is a mouse move event, false if it's a button or wheel event
        bool is_move{false};
    };
}