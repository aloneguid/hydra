#include "hid.h"
#include <algorithm>
#include <cstring>
#include <hardware/flash.h>
#include <hardware/sync.h>

using namespace std;

namespace {

constexpr uint32_t kPreferredCentralMagic = 0x43524448; // "HDRC"
constexpr uint32_t kPreferredCentralVersion = 1;
constexpr uint32_t kPreferredCentralFlashOffset = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;
constexpr size_t kBtAddrStringLength = 17;

struct preferred_central_store {
    uint32_t magic;
    uint32_t version;
    char addr[kBtAddrStringLength + 1];
    uint8_t reserved[FLASH_PAGE_SIZE - sizeof(uint32_t) * 2 - (kBtAddrStringLength + 1)];
};

static_assert(sizeof(preferred_central_store) == FLASH_PAGE_SIZE, "preferred central store must fit one flash page");

const preferred_central_store* preferred_central_flash() {
    return reinterpret_cast<const preferred_central_store*>(XIP_BASE + kPreferredCentralFlashOffset);
}

bool is_valid_bt_addr(const std::string& addr) {
    return !addr.empty() && addr.size() <= kBtAddrStringLength;
}

} // namespace

std::vector<hid_central> hid_central::_centrals;
std::map<std::string, std::string> hid_central::random_to_public_addr;
std::map<std::string, std::string> hid_central::addr_to_user_name;
std::string hid_central::preferred_addr;
bool hid_central::preferred_addr_loaded = false;
hid_central hid_central::cc;

void hid_central::disconnect(hci_con_handle_t handle) {
    if(cc.conn == handle) {
        cc = hid_central(); // clear current connection if it was disconnected
    }

    _centrals.erase(std::remove_if(_centrals.begin(), _centrals.end(),
        [handle](const hid_central& hc) { return hc.conn == handle; }), _centrals.end());

    select_current_after_change();
}

void hid_central::unpair(hci_con_handle_t handle) {
    load_preferred_addr();
    hid_central* c = find(handle);
    if (!c) return;

    std::string target_addr = c->addr;

    // disconnect the BLE link
    gap_disconnect(handle);

    // remove from le_device_db by matching address
    for (int i = 0; i < le_device_db_max_count(); i++) {
        bd_addr_t db_addr;
        int db_addr_type;
        sm_key_t irk;
        le_device_db_info(i, &db_addr_type, db_addr, irk);
        if (db_addr_type == BD_ADDR_TYPE_UNKNOWN) continue;
        if (bd_addr_to_str(db_addr) == target_addr) {
            le_device_db_remove(i);
            break;
        }
    }

    // remove cached name
    addr_to_user_name.erase(target_addr);

    if (target_addr == preferred_addr) {
        clear_preferred_addr();
    }

    // disconnect handles internal list cleanup
    disconnect(handle);
}

hid_central hid_central::connect(hci_con_handle_t handle, const bd_addr_t addr, uint8_t addr_type) {
    load_preferred_addr();
    string saddr = bd_addr_to_str(addr);

    // if address is random, try to resolve it to public address
    if(addr_type == BD_ADDR_TYPE_LE_RANDOM) {
        auto it = random_to_public_addr.find(saddr);
        if(it != random_to_public_addr.end()) {
            saddr = it->second; // use public address if available
            addr_type = BD_ADDR_TYPE_LE_PUBLIC; // change address type to public
        }
    }

    hid_central new_central;
    new_central.conn = handle;
    new_central.addr = saddr;
    new_central.addr_t = addr_type;

    auto it = addr_to_user_name.find(saddr);
    if (it != addr_to_user_name.end()) {
        new_central.name = it->second;
    }

    _centrals.push_back(new_central);

    if (!preferred_addr.empty() && new_central.addr == preferred_addr) {
        cc = new_central;
    } else if (!cc) {
        cc = new_central;
        if (preferred_addr.empty()) {
            store_preferred_addr(new_central.addr);
        }
    }
    return new_central;
}

hid_central* hid_central::find(hci_con_handle_t handle) {
    for (auto& c : _centrals) {
        if (c.conn == handle) return &c;
    }
    return nullptr;
}

void hid_central::set_name(hci_con_handle_t handle, const std::string& name) {
    hid_central* c = find(handle);
    if (!c) return;
    c->name = name;
    addr_to_user_name[c->addr] = name;
    if (cc.conn == handle) cc.name = name;
}

hid_central& hid_central::current() {
    return cc;
}

void hid_central::current(hid_central central, bool persist_preference) {
    cc = central;
    if (persist_preference) {
        store_preferred_addr(central.addr);
    }
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
    load_preferred_addr();
    random_to_public_addr[random_addr] = public_addr;

    // check if any existing central has this random address, and if so update it to use the public address
    for (auto& hc : _centrals) {
        if (hc.addr == random_addr) {
            hc.addr = public_addr;
            hc.addr_t = BD_ADDR_TYPE_LE_PUBLIC;
            if (cc.conn == hc.conn || (!preferred_addr.empty() && public_addr == preferred_addr)) {
                cc = hc;
            }
        }
    }
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

void hid_central::load_preferred_addr() {
    if (preferred_addr_loaded) return;
    preferred_addr_loaded = true;
    preferred_addr.clear();

    const preferred_central_store* store = preferred_central_flash();
    if (store->magic != kPreferredCentralMagic || store->version != kPreferredCentralVersion) {
        return;
    }

    size_t len = strnlen(store->addr, sizeof(store->addr));
    if (len == 0 || len >= sizeof(store->addr)) {
        return;
    }

    preferred_addr.assign(store->addr, len);
}

void hid_central::store_preferred_addr(const std::string& addr) {
    load_preferred_addr();

    if (!is_valid_bt_addr(addr)) {
        return;
    }
    if (preferred_addr == addr) {
        return;
    }

    preferred_central_store page{};
    memset(&page, 0xFF, sizeof(page));
    page.magic = kPreferredCentralMagic;
    page.version = kPreferredCentralVersion;
    memcpy(page.addr, addr.c_str(), addr.size());
    page.addr[addr.size()] = '\0';

    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(kPreferredCentralFlashOffset, FLASH_SECTOR_SIZE);
    flash_range_program(kPreferredCentralFlashOffset, reinterpret_cast<const uint8_t*>(&page), sizeof(page));
    restore_interrupts(interrupts);

    preferred_addr = addr;
}

void hid_central::clear_preferred_addr() {
    load_preferred_addr();

    if (preferred_addr.empty()) {
        return;
    }

    uint8_t erased_page[FLASH_PAGE_SIZE];
    memset(erased_page, 0xFF, sizeof(erased_page));

    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(kPreferredCentralFlashOffset, FLASH_SECTOR_SIZE);
    flash_range_program(kPreferredCentralFlashOffset, erased_page, sizeof(erased_page));
    restore_interrupts(interrupts);

    preferred_addr.clear();
}

void hid_central::select_current_after_change() {
    load_preferred_addr();

    if (_centrals.empty()) {
        cc = hid_central();
        return;
    }

    if (!preferred_addr.empty()) {
        auto preferred = std::find_if(_centrals.begin(), _centrals.end(), [](const hid_central& hc) {
            return hc.addr == preferred_addr;
        });
        if (preferred != _centrals.end()) {
            cc = *preferred;
            return;
        }
    }

    cc = _centrals.front();
}

