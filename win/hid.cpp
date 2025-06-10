#include "hid.h"
#include <Windows.h>
#include <vector>

namespace hydra {

    using namespace std;

    // see chapter 10 from https://usb.org/sites/default/files/hut1_22.pdf to find mappings between HID usage codes and keycodes

    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-mapvirtualkeyexw can also do the translation

    // maps all know virtual key codes to HID codes
    std::map<uint8_t, uint8_t> hid::vk_to_hid_map = {
        // control keys
        { VK_BACK, 0x2A },
        { VK_TAB, 0x2B },
        { VK_CLEAR, 0x00 },
        { VK_RETURN, 0x28 },
        { VK_SHIFT, 0xE1 },
        { VK_CONTROL, 0xE0 },
        { VK_MENU, 0xE2 },
        { VK_PAUSE, 0x48 },
        { VK_CAPITAL, 0x39 },   // caps lock
        { VK_ESCAPE, 0x29 },
        { VK_SPACE, 0x2C },
        { VK_PRIOR, 0x4B },     // page up
        { VK_NEXT, 0x4E },      // page down
        { VK_END, 0x4D },
        { VK_HOME, 0x4A },
        { VK_LEFT, 0x50 },
        { VK_UP, 0x52 },
        { VK_RIGHT, 0x4F },
        { VK_DOWN, 0x51 },
        { VK_SELECT, 0x29 },
        { VK_PRINT, 0x46 },
        { VK_EXECUTE, 0x74 },
        { VK_SNAPSHOT, 0x46 },
        { VK_INSERT, 0x49 },
        { VK_DELETE, 0x4C },
        { VK_HELP, 0x75 },
        { VK_LWIN, 0xE3 },
        { VK_RWIN, 0xE7 },
        { VK_APPS, 0x65 },
        { VK_SLEEP, 0x82 },

        // number row
        { 0x30, 0x27 },     // 0
        { 0x31, 0x1E },     // 1
        { 0x32, 0x1F }, 
        { 0x33, 0x20 },
        { 0x34, 0x21 },
        { 0x35, 0x22 },
        { 0x36, 0x23 },
        { 0x37, 0x24 },
        { 0x38, 0x25 },
        { 0x39, 0x26 },

        // letters
        { 0x41, 0x04 },     // A
        { 0x42, 0x05 },
        { 0x43, 0x06 },
        { 0x44, 0x07 },
        { 0x45, 0x08 },
        { 0x46, 0x09 },
        { 0x47, 0x0A },
        { 0x48, 0x0B },
        { 0x49, 0x0C },
        { 0x4A, 0x0D },
        { 0x4B, 0x0E },     // K
        { 0x4C, 0x0F },
        { 0x4D, 0x10 },
        { 0x4E, 0x11 },
        { 0x4F, 0x12 },
        { 0x50, 0x13 },
        { 0x51, 0x14 },
        { 0x52, 0x15 },
        { 0x53, 0x16 },
        { 0x54, 0x17 },     // T
        { 0x55, 0x18 },
        { 0x56, 0x19 },
        { 0x57, 0x1A },
        { 0x58, 0x1B },
        { 0x59, 0x1C },
        { 0x5A, 0x1D },     // Z

        // numpad
        { VK_NUMPAD0, 0x62 },
        { VK_NUMPAD1, 0x59 },
        { VK_NUMPAD2, 0x5A },
        { VK_NUMPAD3, 0x5B },
        { VK_NUMPAD4, 0x5C },
        { VK_NUMPAD5, 0x5D },
        { VK_NUMPAD6, 0x5E },
        { VK_NUMPAD7, 0x5F },
        { VK_NUMPAD8, 0x60 },
        { VK_NUMPAD9, 0x61 },
        { VK_MULTIPLY, 0x55 },
        { VK_ADD, 0x57 },

        //
        { VK_SEPARATOR, 0x53 },
        { VK_SUBTRACT, 0x56 },
        { VK_DECIMAL, 0x63 },
        { VK_DIVIDE, 0x54 },

        // function keys
        { VK_F1, 0x3A },
        { VK_F2, 0x3B },
        { VK_F3, 0x3C },
        { VK_F4, 0x3D },
        { VK_F5, 0x3E },
        { VK_F6, 0x3F },
        { VK_F7, 0x40 },
        { VK_F8, 0x41 },
        { VK_F9, 0x42 },
        { VK_F10, 0x43 },
        { VK_F11, 0x44 },
        { VK_F12, 0x45 },
        { VK_F13, 0x68 },
        { VK_F14, 0x69 },
        { VK_F15, 0x6A },
        { VK_F16, 0x6B },
        { VK_F17, 0x6C },
        { VK_F18, 0x6D },
        { VK_F19, 0x6E },
        { VK_F20, 0x6F },
        { VK_F21, 0x70 },
        { VK_F22, 0x71 },
        { VK_F23, 0x72 },
        { VK_F24, 0x73 },

        // the rest
        { VK_NUMLOCK, 0x53 },
        { VK_SCROLL, 0x47 },
        { VK_LSHIFT, 0xE1 },
        { VK_RSHIFT, 0xE5 },
        { VK_LCONTROL, 0xE0 },
        { VK_RCONTROL, 0xE4 },
        { VK_LMENU, 0xE2 },
        { VK_RMENU, 0xE6 },
        { VK_BROWSER_BACK, 0x0C },
        { VK_BROWSER_FORWARD, 0x0D },
        { VK_BROWSER_REFRESH, 0x0E },
        { VK_BROWSER_STOP, 0x0F },
        { VK_BROWSER_SEARCH, 0x10 },
        { VK_BROWSER_FAVORITES, 0x11 },
        { VK_BROWSER_HOME, 0x12 },
        { VK_VOLUME_MUTE, 0x7F },
        { VK_VOLUME_DOWN, 0x81 },
        { VK_VOLUME_UP, 0x80 },
        { VK_MEDIA_NEXT_TRACK, 0x4B },
        { VK_MEDIA_PREV_TRACK, 0x4C },
        { VK_MEDIA_STOP, 0x24 },
        { VK_MEDIA_PLAY_PAUSE, 0x34 },
        { VK_LAUNCH_MAIL, 0x18 },
        { VK_LAUNCH_MEDIA_SELECT, 0x16 },
        { VK_LAUNCH_APP1, 0x1A },
        { VK_LAUNCH_APP2, 0x1B },
        { VK_OEM_1, 0x33 },     // ;:
        { VK_OEM_PLUS, 0x2E },  // =+
        { VK_OEM_COMMA, 0x36 }, // ,<
        { VK_OEM_MINUS, 0x2D }, // -_
        { VK_OEM_PERIOD, 0x37 },// .>
        { VK_OEM_2, 0x38 },     // /?
        { VK_OEM_3, 0x35 },     // `~
        { VK_OEM_4, 0x2F },     // [{
        { VK_OEM_5, 0x31 },     // \|
        { VK_OEM_6, 0x30 },     // ]}
        { VK_OEM_7, 0x34 },     // '"
        { VK_OEM_8, 0x27 },     // misc
        { VK_OEM_102, 0x64 },   // <>
        { VK_ATTN, 0x9A },
        { VK_CRSEL, 0xA3 },
        { VK_EXSEL, 0xA4 },
        { VK_EREOF, 0xA5 },
        { VK_PLAY, 0xA6 },
        { VK_ZOOM, 0xA7 },
        { VK_PA1, 0xA8 },
        { VK_OEM_CLEAR, 0xA9 }
    };

    // maps all known typeable characters to HID codes with optional modifiers
    map<uint8_t, pair<uint8_t, uint8_t>> hid::char_to_hid_map = {
        // lowercase letters
        { 'a', { 0x04, 0x00 } },
        { 'b', { 0x05, 0x00 } },
        { 'c', { 0x06, 0x00 } },
        { 'd', { 0x07, 0x00 } },
        { 'e', { 0x08, 0x00 } },
        { 'f', { 0x09, 0x00 } },
        { 'g', { 0x0A, 0x00 } },
        { 'h', { 0x0B, 0x00 } },
        { 'i', { 0x0C, 0x00 } },
        { 'j', { 0x0D, 0x00 } },
        { 'k', { 0x0E, 0x00 } },
        { 'l', { 0x0F, 0x00 } },
        { 'm', { 0x10, 0x00 } },
        { 'n', { 0x11, 0x00 } },
        { 'o', { 0x12, 0x00 } },
        { 'p', { 0x13, 0x00 } },
        { 'q', { 0x14, 0x00 } },
        { 'r', { 0x15, 0x00 } },
        { 's', { 0x16, 0x00 } },
        { 't', { 0x17, 0x00 } },
        { 'u', { 0x18, 0x00 } },
        { 'v', { 0x19, 0x00 } },
        { 'w', { 0x1A, 0x00 } },
        { 'x', { 0x1B, 0x00 } },
        { 'y', { 0x1C, 0x00 } },
        { 'z', { 0x1D, 0x00 } },

        // uppercase letters
        // note: uppercase letters require the left shift modifier, so the second value is 0x02
        { 'A', { 0x04, 0x02 } },
        { 'B', { 0x05, 0x02 } },
        { 'C', { 0x06, 0x02 } },
        { 'D', { 0x07, 0x02 } },
        { 'E', { 0x08, 0x02 } },
        { 'F', { 0x09, 0x02 } },
        { 'G', { 0x0A, 0x02 } },
        { 'H', { 0x0B, 0x02 } },
        { 'I', { 0x0C, 0x02 } },
        { 'J', { 0x0D, 0x02 } },
        { 'K', { 0x0E, 0x02 } },
        { 'L', { 0x0F, 0x02 } },
        { 'M', { 0x10, 0x02 } },
        { 'N', { 0x11, 0x02 } },
        { 'O', { 0x12, 0x02 } },
        { 'P', { 0x13, 0x02 } },
        { 'Q', { 0x14, 0x02 } },
        { 'R', { 0x15, 0x02 } },
        { 'S', { 0x16, 0x02 } },
        { 'T', { 0x17, 0x02 } },
        { 'U', { 0x18, 0x02 } },
        { 'V', { 0x19, 0x02 } },
        { 'W', { 0x1A, 0x02 } },
        { 'X', { 0x1B, 0x02 } },
        { 'Y', { 0x1C, 0x02 } },
        { 'Z', { 0x1D, 0x02 } },

        // numbers
        { '0', { 0x27, 0x00 } },
        { '1', { 0x1E, 0x00 } },
        { '2', { 0x1F, 0x00 } },
        { '3', { 0x20, 0x00 } },
        { '4', { 0x21, 0x00 } },
        { '5', { 0x22, 0x00 } },
        { '6', { 0x23, 0x00 } },
        { '7', { 0x24, 0x00 } },
        { '8', { 0x25, 0x00 } },
        { '9', { 0x26, 0x00 } },

        // special characters
        { ' ', { 0x2C, 0x00 } }, // space
        { '-', { 0x2D, 0x00 } }, // hyphen
        { '=', { 0x2E, 0x00 } }, // equals
        { '[', { 0x2F, 0x00 } }, // left bracket
        { ']', { 0x30, 0x00 } }, // right bracket
        { '\\', { 0x31, 0x00 } }, // backslash
        { ';', { 0x33, 0x00 } }, // semicolon
        { '\'', { 0x34, 0x00 } }, // apostrophe
        { '`', { 0x35, 0x00 } }, // grave accent
        { ',', { 0x36, 0x00 } }, // comma
        { '.', { 0x37, 0x00 } }, // period
        { '/', { 0x38, 0x00 } }, // forward slash
        { '!', { 0x1E, 0x02 } }, // exclamation mark (shift + 1)
        { '@', { 0x1F, 0x02 } }, // at symbol (shift + 2)
        { '#', { 0x20, 0x02 } }, // hash (shift + 3)
        { '$', { 0x21, 0x02 } }, // dollar sign (shift + 4)
        { '%', { 0x22, 0x02 } }, // percent (shift + 5)
        { '^', { 0x23, 0x02 } }, // caret (shift + 6)
        { '&', { 0x24, 0x02 } }, // ampersand (shift + 7)
        { '*', { 0x25, 0x02 } }, // asterisk (shift + 8)
        { '(', { 0x26, 0x02 } }, // left parenthesis (shift + 9)
        { ')', { 0x27, 0x02 } }, // right parenthesis (shift + 0)
        { '_', { 0x2D, 0x02 } }, // underscore (shift + -)
        { '+', { 0x2E, 0x02 } }, // plus (shift + =)
        { '{', { 0x2F, 0x02 } }, // left brace (shift + [)
        { '}', { 0x30, 0x02 } }, // right brace (shift + ])
        { '|', { 0x31, 0x02 } }, // pipe (shift + \)
        { ':', { 0x33, 0x02 } }, // colon (shift + ;)
        { '"', { 0x34, 0x02 } }, // double quote (shift + ')
        { '~', { 0x35, 0x02 } }, // tilde (shift + `)
        { '<', { 0x36, 0x02 } }, // less than (shift + ,)
        { '>', { 0x37, 0x02 } }, // greater than (shift + .)
        { '?', { 0x38, 0x02 }}, // question mark (shift + /)
        { '\n', { 0x28, 0x00 }}, // enter key
    };

    uint8_t hid::vk_to_hid(uint8_t vk) {

        // translate virtual key code to hid

        auto it = vk_to_hid_map.find(vk);
        if(it != vk_to_hid_map.end()) {
            return it->second;
        }

        return 0;
    }

    bool hid::char_to_hid(uint8_t ch, uint8_t& modifier, uint8_t& hid_code) {
        auto it = char_to_hid_map.find(ch);
        if(it != char_to_hid_map.end()) {
            hid_code = it->second.first; // get the HID code
            modifier = it->second.second; // get the modifier (if any)
            return true;
        }
        // if the character is not found, return false
        return false;
    }

    bool hid::is_any_key_pressed(bool check_keyboard, bool check_mouse) {

        if (check_keyboard) {
            for (int vk = 0x08; vk <= 0xFE; ++vk) { // 0x08 = VK_BACK, 0xFE = last defined VK
                SHORT state = ::GetAsyncKeyState(vk);
                if (state & 0x8000) // high-order bit set if key is down
                    return true;
            }
        }

        if(check_mouse) {
            // Check mouse buttons
            if(::GetAsyncKeyState(VK_LBUTTON) & 0x8000) return true; // left button
            if(::GetAsyncKeyState(VK_RBUTTON) & 0x8000) return true; // right button
            if(::GetAsyncKeyState(VK_MBUTTON) & 0x8000) return true; // middle button
            if(::GetAsyncKeyState(VK_XBUTTON1) & 0x8000) return true; // X1 button
            if(::GetAsyncKeyState(VK_XBUTTON2) & 0x8000) return true; // X2 button
        }

        return false;
    }

    bool hid::set_hid_modifier(uint8_t vk, uint8_t& mod_output) {

        switch(vk) {
            case VK_LCONTROL:
                mod_output |= 0x01; // left control
                return true;
            case VK_RCONTROL:
                mod_output |= 0x10; // right control
                return true;
            case VK_LSHIFT:
                mod_output |= 0x02; // left shift
                return true;
            case VK_RSHIFT:
                mod_output |= 0x20; // right shift
                return true;
            case VK_LMENU:
                mod_output |= 0x04; // left alt
                return true;
            case VK_RMENU:
                mod_output |= 0x40; // right alt
                return true;
            case VK_LWIN:
                mod_output |= 0x08; // left GUI (Windows key)
                return true;
            case VK_RWIN:
                mod_output |= 0x80; // right GUI (Windows key)
                return true;
        }

        return false;
    }


}