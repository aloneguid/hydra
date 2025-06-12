#include "hid.h"
#include <algorithm>
#include <hardware/flash.h>

using namespace std;

std::vector<hid_central> hid_central::_centrals;
std::map<std::string, std::string> hid_central::random_to_public_addr;
std::map<std::string, std::string> hid_central::addr_to_user_name;
hid_central hid_central::cc;

void hid_central::disconnect(hci_con_handle_t handle) {
    if(cc.conn == handle) {
        cc = hid_central(); // clear current connection if it was disconnected
    }

    _centrals.erase(std::remove_if(_centrals.begin(), _centrals.end(),
        [handle](const hid_central& hc) { return hc.conn == handle; }), _centrals.end());
}

hid_central hid_central::connect(hci_con_handle_t handle, const bd_addr_t addr, uint8_t addr_type) {
    string saddr = bd_addr_to_str(addr);

    // if address is random, try to resolve it to public address
    if(addr_type == BD_ADDR_TYPE_LE_RANDOM) {
        auto it = random_to_public_addr.find(saddr);
        if(it != random_to_public_addr.end()) {
            saddr = it->second; // use public address if available
            addr_type = BD_ADDR_TYPE_LE_PUBLIC; // change address type to public
        }
    }

    hid_central new_central{handle, saddr, addr_type};
    _centrals.push_back(new_central);

    if(_centrals.size() == 1) {
        cc = new_central; // set current connection handle to the newly connected device if this is the first connection
    }
    return new_central;
}

hid_central& hid_central::current() {
    return cc;
}

void hid_central::current(hid_central central) {
    cc = central;
}

bool hid_central::contains(hci_con_handle_t handle) {
    return std::any_of(_centrals.begin(), _centrals.end(),
        [handle](const hid_central& hc) { return hc.conn == handle; });
}

vector<hid_central>& hid_central::centrals() {
    refresh_irks();
    return _centrals;
}

void hid_central::add_address_mapping(const std::string& random_addr, const std::string& public_addr) {
    random_to_public_addr[random_addr] = public_addr;
}

void hid_kbd_rpt_set_keycode(uint8_t* report, uint8_t keycode) {
    report[0] = 0; // set modifier
    report[1] = 0; // reserved
    report[2] = keycode; // set keycode
    report[3] = 0; // reserved
    report[4] = 0; // reserved
    report[5] = 0; // reserved
    report[6] = 0; // reserved
    report[7] = 0; // reserved
}

void hid_kbd_rpt_mouse_up(uint8_t* report) {
    report[0] = 0; // buttons
    report[1] = 0; // X
    report[2] = -1 * 10; // Y * speed
    report[3] = 0; // wheel
}

std::string hid_central::addr_type_to_str(uint8_t addr_type) {
    switch(addr_type) {
        case BD_ADDR_TYPE_LE_PUBLIC: return "public";
        case BD_ADDR_TYPE_LE_RANDOM: return "random";
        default: return std::to_string(addr_type);
    }
}

std::string hid_central::addr_to_str(const bd_addr_t& addr) {
    return bd_addr_to_str(addr);
}

std::string hid_central::to_string(sm_key_t irk) {
    std::string result;
    for (int i = 0; i < sizeof(sm_key_t); i++) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", irk[i]);
        result += buf;
    }
    return result;
}

void hid_central::clear_device_db() {
    for (int i = 0; i < le_device_db_max_count(); i++) {
        printf("%u\n", i);
        le_device_db_remove(i);
    }
}

void hid_central::refresh_irks() {
    for (auto& hc : _centrals) {

        // find entry in device db using the address
        for(int i = 0; i < le_device_db_max_count(); i++) {
            bd_addr_t addr;
            int addr_type;
            sm_key_t irk;
            le_device_db_info(i, &addr_type, addr, irk);
            if (addr_type == BD_ADDR_TYPE_UNKNOWN) continue; // skip unknown addresses
            string addr_s = bd_addr_to_str(addr);
            if (addr_s == addr_s) {
                hc.irk = to_string(irk);
                break; // found the entry
            }
        }
    }
}

