#pragma once
#include "pico/stdlib.h"
#include "btstack.h"
#include <map>
#include <string>

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

    //
    0x05, 0x0C,   // Usage Pg (Consumer Devices)
    0x09, 0x01,   // Usage (Consumer Control)
    0xA1, 0x01,   // Collection (Application)
    0x85, 0x02,   // Report Id (2)
    0x05, 0x0C,   //   Usage Pg (Consumer Devices)
    0x09, 0x86,   //   Usage (Channel)
    0x15, 0xFF,   //   Logical Min (-1)
    0x25, 0x01,   //   Logical Max (1)
    0x75, 0x02,   //   Report Size (2)
    0x95, 0x01,   //   Report Count (1)
    0x81, 0x46,   //   Input (Data, Var, Rel, Null)
    0x09, 0xE9,   //   Usage (Volume Up)
    0x09, 0xEA,   //   Usage (Volume Down)
    0x15, 0x00,   //   Logical Min (0)
    0x75, 0x01,   //   Report Size (1)
    0x95, 0x02,   //   Report Count (2)
    0x81, 0x02,   //   Input (Data, Var, Abs)
    0x09, 0xE2,   //   Usage (Mute)
    0x09, 0x30,   //   Usage (Power)
    0x09, 0x83,   //   Usage (Recall Last)
    0x09, 0x81,   //   Usage (Assign Selection)
    0x09, 0xB0,   //   Usage (Play)
    0x09, 0xB1,   //   Usage (Pause)
    0x09, 0xB2,   //   Usage (Record)
    0x09, 0xB3,   //   Usage (Fast Forward)
    0x09, 0xB4,   //   Usage (Rewind)
    0x09, 0xB5,   //   Usage (Scan Next)
    0x09, 0xB6,   //   Usage (Scan Prev)
    0x09, 0xB7,   //   Usage (Stop)
    0x15, 0x01,   //   Logical Min (1)
    0x25, 0x0C,   //   Logical Max (12)
    0x75, 0x04,   //   Report Size (4)
    0x95, 0x01,   //   Report Count (1)
    0x81, 0x00,   //   Input (Data, Ary, Abs)
    0xC0,            // End Collection

    // --- Mouse ---
    // we want mouse report to be 4 bytes long (1 byte for buttons, 1 byte for X, Y, and wheel movement)

    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x02,  // Usage (Mouse)
    0xA1, 0x01,  // Collection (Application)
        0x85, 0x03,  // Report Id (3)
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

    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x05,  // Usage (Gamepad)
    0xA1, 0x01,  // Collection (Application)
    0x85, 0x04,  // Report Id (4)
    /* 8 bit X, Y, Z, Rz, Rx, Ry (min -127, max 127 ) */
    /* implemented like Gamepad from tinyUSB */
      0x05, 0x01,  // Usage Page (Generic Desktop)
      0x09, 0x30,  // Usage (desktop X)
      0x09, 0x31,  // Usage (desktop Y)
      0x09, 0x32,  // Usage (desktop Z)
      0x09, 0x35,  // Usage (desktop RZ)
      0x09, 0x33,  // Usage (desktop RX)
      0x09, 0x34,  // Usage (desktop RY)
      0x15, 0x81,  // Logical Minimum (-127)
      0x25, 0x7F,  // Logical Maximum (127)
      0x95, 0x06,  // Report Count (6)
      0x75, 0x08,  // Report Size (8)
      0x81, 0x02,  // Input: (Data, Variable, Absolute)
      /* 8 bit DPad/Hat Button Map  */
        0x05, 0x01,  // Usage Page (Generic Desktop)
        0x09, 0x39,  // Usage (hat switch)
        0x15, 0x01,   //     Logical Min (1)
        0x25, 0x08,   //     Logical Max (8)

        0x35, 0x00,   // Physical minimum (0)
        0x46, 0x3B, 0x01, // Physical maximum (315, size 2)
        //0x46, 0x00,   // Physical maximum (315, size 2)
        0x95, 0x01,  // Report Count (1)
        0x75, 0x08,  // Report Size (8)
        0x81, 0x02,  // Input: (Data, Variable, Absolute)
        /* 16 bit Button Map */
          0x05, 0x09,  // Usage Page (button)
          0x19, 0x01,  //     Usage Minimum (01) - Button 1
          0x29, 0x20,  //     Usage Maximum (32) - Button 32
          0x15, 0x00,   //     Logical Min (0)
          0x25, 0x01,   //     Logical Max (1)
          0x95, 0x20,  // Report Count (32)
          0x75, 0x01,  // Report Size (1)
          0x81, 0x02,  // Input: (Data, Variable, Absolute)
        0xC0,            // End Collection
};

void hid_kbd_rpt_set_keycode(uint8_t* rpt, uint8_t keycode);
void hid_kbd_rpt_mouse_up(uint8_t* rpt);

class hid_central {
    public:
        hci_con_handle_t conn{ HCI_CON_HANDLE_INVALID };
        std::string addr;
        uint8_t addr_t;
        std::string irk; // Identity Resolving Key, used for resolving random addresses

        // globals
        static hci_con_handle_t current_conn_handle;
        
        static void disconnect(hci_con_handle_t handle);
        static hid_central connect(hci_con_handle_t handle, const bd_addr_t addr, uint8_t addr_type);
        static hid_central& current();
        static bool any() {
            return !instances.empty();
        }
        static size_t size() {
            return instances.size();
        }
        static std::map<hci_con_handle_t, hid_central>& get_instances();

        static void add_address_mapping(const std::string& random_addr, const std::string& public_addr);

        //utils
        static std::string addr_type_to_str(uint8_t addr_type);
        static std::string addr_to_str(const bd_addr_t& addr);
        static std::string to_string(sm_key_t irk);
        static void clear_device_db();

    private:
        static std::map<hci_con_handle_t, hid_central> instances;
        static std::map<std::string, std::string> random_to_public_addr;

        /**
         * Iterates through instances and refreshes IRKs for all devices using device db;
         */
        static void refresh_irks();
};