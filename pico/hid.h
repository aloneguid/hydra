#pragma once
#include "pico/stdlib.h"
#include "btstack.h"
#include <map>
#include <string>
#include <vector>

/**
 * Understand HID descriptors
 * - Report Count lists number of reports after.
 * - Repost Size is in bits.
 * - Input means "process what you've seen so far". Input is a bit mask that is sent to the host.
 *   0 - data (0) or constant (1)
 *   1 - array (0) or variable (1)
 *   2 - absolute (0) or relative (1)
 *   other bits are less interesting
 * Links:
 * - https://who-t.blogspot.com/2018/12/understanding-hid-report-descriptors.html
 */

/**
 * Reports IDs:
 * 1. Keyboard
 * 2. Consumer Control
 * 3. Mouse
 * 4. Gamepad (Joystick)
 * 5. Mouse with absolute positioning
 */

static const uint8_t HidReportMap[] = {

    // --- Keyboard ---
    // we want keyboard report to be 8 bytes long (hence 1 reserved byte)
    // format: 1 modifier byte, 1 reserved byte, 6 key codes

    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection: (Applicatsion)
        0x85, 0x01,  // Report Id (1)
        0x05, 0x07,  // Usage Page (Keyboard)
            0x19, 0xE0,  // Usage Min (224 - left ctrl). This means 1st bit is left ctrl.
            0x29, 0xE7,  // Usage Max (231 - right GUI). This means last bit is right GUI. 231 - 224 = 7, so we have 8 bits in total.
            0x15, 0x00,  // Logical Minimum (0)
            0x25, 0x01,  // Logical Maximum (1)

            // Modifier byte
            0x75, 0x01,  // Report Size (1)
            0x95, 0x08,  // Report Count (8) ; 8 bits
            0x81, 0x02,  // Input: (Data, Variable, Absolute) ; Modifier byte

            // Reserved byte (for 8-byte alignment)
            0x75, 0x01,  // Report Size (1)
            0x95, 0x08,  // Report Count (8) ; 8 bits
            0x81, 0x01,  // Input: (Constant) ; Reserved byte

            // Array of keys (6 bytes)
            // This means we can send up to 6 keys at once.
            0x95, 0x06,  // Report Count (6)
            0x75, 0x08,  // Report Size (8)
            0x15, 0x00,  // Log Min (0)
            0x25, 0x65,  // Log Max (101)
            0x05, 0x07,  // Usage Pg (Key Codes)
            0x19, 0x00,  // Usage Min (0)
            0x29, 0x65,  // Usage Max (101)
            0x81, 0x00,  // Input: (Data, Array)
    0xC0,        // End Collection

    // --- Mouse ---
    // we want mouse report to be 4 bytes long (1 byte for buttons, 1 byte for X, Y, and wheel movement)

    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x02,  // Usage (Mouse)
    0xA1, 0x01,  // Collection (Application)
        0x85, 0x02,  // Report Id (2)
        0x09, 0x01,  // Usage (Pointer)
        0xA1, 0x00,  // Collection (Physical)

            // 3 buttons (1 bit each) padded with 5 bits - 1 byte total
            0x05, 0x09,  // Usage Page (Buttons)
            0x19, 0x01,  // Usage Minimum (01) - Button 1
            0x29, 0x03,  // Usage Maximum (03) - Button 3
            0x15, 0x00,  // Logical Minimum (0)
            0x25, 0x01,  // Logical Maximum (1)
            0x75, 0x01,  // Report Size (1)
            0x95, 0x03,  // Report Count (3)
            0x81, 0x02,  // Input (Data, Variable, Absolute) - Button states
            0x75, 0x05,  // Report Size (5)
            0x95, 0x01,  // Report Count (1)
            0x81, 0x01,  // Input (Constant) - Padding or Reserved bits

            // X, Y, and Wheel movement, 1 byte each
            0x05, 0x01,  // Usage Page (Generic Desktop)
            0x09, 0x30,  // Usage (X)
            0x09, 0x31,  // Usage (Y)
            0x09, 0x38,  // Usage (Wheel)
            0x15, 0x81,  // Logical Minimum (-127)
            0x25, 0x7F,  // Logical Maximum (127)
            0x75, 0x08,  // Report Size (8)
            0x95, 0x03,  // Report Count (3)
            0x81, 0x06,  // Input (Data, Variable, Relative) - X & Y coordinate

        0xC0,        //   End Collection
    0xC0,        // End Collection

    // --- Mouse with Absolute Positioning ---
    // we want mouse report to be 5 bytes long (1 byte for buttons, 2 bytes for X, 2 bytes for Y)
    // 0x05, 0x01,  // Usage Page (Generic Desktop)
    // 0x09, 0x02,  // Usage (Mouse)
    // 0xA1, 0x01,  // Collection (Application)
    //     0x85, 0x05,  // Report Id (5)
    //     0x09, 0x01,  // Usage (Pointer)
    //     0xA1, 0x00,  // Collection (Physical)

    //         // 3 buttons (1 bit each) padded with 5 bits - 1 byte total
    //         0x05, 0x09,  // Usage Page (Buttons)
    //         0x19, 0x01,  // Usage Minimum (01) - Button 1
    //         0x29, 0x03,  // Usage Maximum (03) - Button 3
    //         0x15, 0x00,  // Logical Minimum (0)
    //         0x25, 0x01,  // Logical Maximum (1)
    //         0x75, 0x01,  // Report Size (1)
    //         0x95, 0x03,  // Report Count (3)
    //         0x81, 0x02,  // Input (Data, Variable, Absolute) - Button states
    //         0x75, 0x05,  // Report Size (5)
    //         0x95, 0x01,  // Report Count (1)
    //         0x81, 0x01,  // Input (Constant) - Padding or Reserved bits

    //         // absolute X, Y, 2 bytes each
    //         0x05, 0x01,  // Usage Page (Generic Desktop)
    //         0x09, 0x30,  // Usage (X)
    //         0x09, 0x31,  // Usage (Y)
    //         0x15, 0x00,        // Logical Minimum (0)
    //         0x26, 0xFF, 0x03,  // Logical Maximum (1023)
    //         0x75, 0x10,        // Report Size (16)
    //         0x95, 0x02,        // Report Count (2)
    //         0x81, 0x02,        // Input (Data, Variable, Absolute) - Absolute position
    //     0xC0,        //   End Collection
    // 0xC0,        // End Collection
};

void hid_kbd_rpt_set_keycode(uint8_t* rpt, uint8_t keycode);
void hid_kbd_rpt_mouse_up(uint8_t* rpt);

class hid_central {
    public:
        hci_con_handle_t conn{ HCI_CON_HANDLE_INVALID };
        std::string addr;
        uint8_t addr_t;
        std::string irk; // Identity Resolving Key, used for resolving random addresses

        operator bool() const { return conn != HCI_CON_HANDLE_INVALID; }

        // globals
        static void disconnect(hci_con_handle_t handle);
        static hid_central connect(hci_con_handle_t handle, const bd_addr_t addr, uint8_t addr_type);
        static hid_central& current();
        static void current(hid_central central);
        static bool any() {
            return !_centrals.empty();
        }
        static bool contains(hci_con_handle_t handle);
        
        static size_t size() {
            return _centrals.size();
        }
        static std::vector<hid_central>& centrals();

        static void add_address_mapping(const std::string& random_addr, const std::string& public_addr);

        //utils
        static std::string addr_type_to_str(uint8_t addr_type);
        static std::string addr_to_str(const bd_addr_t& addr);
        static std::string to_string(sm_key_t irk);
        static void clear_device_db();

    private:
        static hid_central cc;
        static std::vector<hid_central> _centrals;
        static std::map<std::string, std::string> random_to_public_addr;
        static std::map<std::string, std::string> addr_to_user_name;

        /**
         * Iterates through instances and refreshes IRKs for all devices using device db;
         */
        static void refresh_irks();
};