#pragma once
#include <map>
#include <vector>

namespace hydra {

    // all HID codes are in Chapter 10 https://usb.org/sites/default/files/hut1_22.pdf
    // all VK codes

    class hid {
    public:
        /**
         * @brief Translate a virtual key code to a HID code.
         * @param vk 
         * @return HID code corresponding to the virtual key code, or 0 if not found.
         */
        static uint8_t vk_to_hid(uint8_t vk);

        static uint8_t vk_to_scancode(uint8_t vk);

        /**
         * @brief if the virtual key code is a modifier key, set the corresponding HID modifier output bit.
         * @param vk 
         * @param mod_output 
         * @return 
         */
        static bool set_hid_modifier(uint8_t vk, uint8_t& mod_output);

        static bool char_to_hid(uint8_t ch, uint8_t& modifier, uint8_t& hid_code);

        /**
         * @brief Checks if any key in the current machine is pressed. Iterates over all keys, therefore not performance friendly.
         * @return 
         */
        static bool is_any_key_pressed(bool check_keyboard = true, bool check_mouse = true);

        static std::vector<uint8_t> get_pressed_keys(bool check_keyboard = true, bool check_mouse = true);

    private:
        static std::map<uint8_t, uint8_t> vk_to_hid_map;
        // pair is (HID keycode, HID modifier)
        static std::map<uint8_t, std::pair<uint8_t, uint8_t>> char_to_hid_map;
    };
}