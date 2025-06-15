#include "dongle.h"
#include <fmt/core.h>
#include "hid.h"
#include <thread>
#include <chrono>
#include "str.h"

namespace hydra {

    using namespace std;

    dongle::dongle() {
        // avoid throwing an exception in the constructor
        //refresh();
    }

    void dongle::refresh() {

        // refresh ports
        if(com_port) com_port.reset();

        auto ports = win32::serial_port::enumerate();
        for(auto& port : ports) {
            if(port.bus_description == "Board CDC") {
                com_port = make_shared<win32::serial_port>(port);
                break;
            }
        }

        // refresh connected devices
        list_centrals();
    }

    std::string dongle::get_port_name() const {
        return connected() ? com_port->name : "";
    }

#if _DEBUG

    void dbg_print(const std::string& prefix, const std::vector<uint8_t>& data) {
        string s = fmt::format("{} ({}): ", prefix, data.size());
        //hex-encode data into string
        for(uint8_t byte : data) {
            s += fmt::format("{:02X} ", byte);
        }
        s += "\n";
        ::OutputDebugStringA(s.c_str());
    }

#endif

    void dongle::send(const std::vector<uint8_t>& data) {
        if(!connected()) return;

#if _DEBUG
        dbg_print("COM <==", data);
#endif

        // send to serial port
        com_port->send(data);

        //#if _DEBUG
                // read serial port output response for debugging
                //{
                //    string response;
                //    port.recv(response, 1000);
                //    ::OutputDebugStringA(
                //        fmt::format("COM out: {}\n", response).c_str());
                //}
        //#endif

    }
    void hydra::dongle::send_kbd_char(char c, size_t delay_ms) {
        uint8_t modifier;
        uint8_t hid_code;

        if(!hid::char_to_hid(c, modifier, hid_code)) {
            return;
        }

        hid_rpt_clear();
        hid_rpt_kbd[1] = modifier; // set the modifier byte
        hid_rpt_kbd[3] = hid_code; // set the HID code in the report

        send(hid_rpt_kbd);

        // wait for a short time to simulate key press duration
        if(delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds{delay_ms});
        }

        // release the key
        hid_rpt_clear();
        send(hid_rpt_kbd);
    }

    void dongle::send_kbd(const std::vector<uint8_t>& vks_down) {
        // clear HID report
        hid_rpt_clear();

        // remake HID report
        size_t keys_filled = 0; // we can fill up to 6 keys in the report
        for(uint8_t vk_code : vks_down) {
            if(!hid::set_hid_modifier(vk_code, hid_rpt_kbd[1])) {

                hid_rpt_kbd[3 + keys_filled] = hid::vk_to_hid(vk_code); // fill HID report

                if(++keys_filled >= 6) {
                    continue; // skip if we already filled 6 keys
                }
            }
        }

        send(hid_rpt_kbd);
    }

    void dongle::send_mouse(bool l_down, bool m_down, bool r_down, int xdiff, int ydiff, int wdiff) {
        hid_rpt_clear();

        // populate first byte for buttons
        if(l_down) {
            hid_rpt_mouse[1] |= 0x01; // left button
        }
        if(r_down) {
            hid_rpt_mouse[1] |= 0x02; // right button
        }
        if(m_down) {
            hid_rpt_mouse[1] |= 0x04; // middle button
        }

        hid_rpt_mouse[2] = static_cast<uint8_t>(xdiff); // x movement
        hid_rpt_mouse[3] = static_cast<uint8_t>(ydiff); // y movement
        hid_rpt_mouse[4] = static_cast<uint8_t>(wdiff); // wheel delta

        send(hid_rpt_mouse);
    }

    void dongle::send_flush() {
        // send empty report to flush the output
        hid_rpt_clear();
        send(hid_rpt_kbd);
        send(hid_rpt_mouse);
    }

    std::vector<ble_central> dongle::list_centrals() {

        vector<ble_central> centrals;

        if(connected()) {
            com_port->purge();
            send(vector<uint8_t>{'l'}); // send the 'list' command to the dongle

            // wait for a response
            string rs;
            com_port->recv(rs, 1000);

            // parse the response, example response:
            /*
            log: command: 108
            #       HANDLE  ADDRESS                 AT      IRK
            0 *     64      F0:CD:31:B0:4F:75       public  a44c2c2fa394e5f046613381d132816a
            */

            vector<string> lines = str::split(rs, "\n", true);
            for(const string& line : lines) {
                // if line does not start with a digit, skip it
                if(line.empty() || !isdigit(line[0])) {
                    continue;
                }

                // split the line by whitespace
                vector<string> parts = str::split(line, "\t", true);

                if(parts.size() == 5) {
                    centrals.emplace_back(parts[2], "", parts[0].ends_with("*"));
                }
            }
        }

        return centrals;
    }

    void dongle::activate_central(const std::string& address) {
        com_port->purge();
        com_port->send("s" + address);
        string r;
        com_port->recv(r, 1000);
    }

    void dongle::get_dashboard() {
        com_port->purge();
        com_port->send("ds"); // send the Dashboard Show command
        string r;
        com_port->recv(r, 1000);
        // parse the response
        
#if _DEBUG
        ::OutputDebugStringA(fmt::format("Dashboard response: {}\n", r).c_str());
#endif
    }

    void dongle::reset_dashboard() {
        com_port->purge();
        com_port->send("dr"); // send the Dashboard Reset command
        string r;
        com_port->recv(r, 1000);
    }

    void hydra::dongle::hid_rpt_clear() {
        std::fill(hid_rpt_kbd.begin() + 1, hid_rpt_kbd.end(), 0);
        std::fill(hid_rpt_mouse.begin() + 1, hid_rpt_mouse.end(), 0);
    }

    void dongle::restart() {
        com_port->purge();
        com_port->send("r"); // send the restart command
    }
}