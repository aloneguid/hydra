#pragma once
#include <vector>
#include <memory>
#include "win32/serial_port.h"

namespace hydra {

    class ble_central {
    public:
        std::string address;
        std::string name;
        bool is_default;
    };

    /**
     * @brief Communication protocol with RP dongle over serial port
     */
    class dongle {
    public:
        dongle();
        
        inline bool connected() const { return (bool)com_port; }

        void refresh();
        std::string get_port_name() const;

        void send(const std::vector<uint8_t>& data);

        /**
         * @brief Types a character on the keyboard with a delay between key press and release.
         * @param c 
         * @param delay_ms 
         */
        void send_kbd_char(char c, size_t delay_ms);

        /**
         * @brief Sends a keyboard report with pressed keys.
         * @param vks_down Virtual key codes of keys to press down.
         */
        void send_kbd(const std::vector<uint8_t>& vks_down);

        /**
         * @brief Sends a mouse report with buttons and movement.
         * @param l_down 
         * @param m_down 
         * @param r_down 
         * @param xdiff 
         * @param ydiff 
         * @param wdiff 
         */
        void send_mouse(bool l_down, bool m_down, bool r_down,
            int xdiff, int ydiff, int wdiff);

        /**
         * @brief Sends clear report to all peripherials so that it looks like nothing is touched.
         */
        void send_flush();

        /**
         * @brief Lists all currently connected BLE centrals.
         * @return 
         */
        std::vector<ble_central> list_centrals();

        void activate_central(const std::string& address);

        void restart();

    private:
        std::shared_ptr<win32::serial_port> com_port; // dongle COM port

        // HID reports, all fixed size and idential to the target device reports, prepended with "command index"

        // 1 modifier byte, 1 reserved byte, 6 key codes
        //                                 0  1  2  3  4  5  6  7  8
        std::vector<uint8_t> hid_rpt_kbd  {1, 0, 0, 0, 0, 0, 0, 0, 0};
        // 1 byte for buttons, 1 byte for X, 1 byte for Y, 1 byte for wheel movement
        //                                 0  1  2  3  4
        std::vector<uint8_t> hid_rpt_mouse{2, 0, 0, 0, 0};

        void hid_rpt_clear();
    };
}