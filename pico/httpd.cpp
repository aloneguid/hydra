#include "httpd.h"
#include "log.h"
#include "secrets.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

// lwip
#include "lwip/ip4_addr.h"
#include "lwip/apps/mdns.h"
#include "lwip/init.h"
#include "lwip/apps/httpd.h"

using namespace std;

httpd* httpd::g_httpd{nullptr};

// ---- Binary command protocol (browser -> device) ----
// Frame layout: [CMD: u8] [payload...]
enum : uint8_t {
    CMD_KBD_REPORT        = 0x01,  // 8 bytes: standard HID keyboard report
    CMD_MOUSE             = 0x02,  // 4 bytes: buttons(u8), dx(i8), dy(i8), wheel(i8)
    CMD_BT_ADV_TOGGLE     = 0x03,  // no payload
    CMD_BT_CENTRAL_ACT    = 0x04,  // u16le: central_id
    CMD_BT_CENTRAL_UNPAIR = 0x05,  // u16le: central_id
    CMD_TYPE              = 0x06,  // u16le: len, then len bytes UTF-8
    CMD_REBOOT            = 0x07,  // no payload
};

static uint16_t rd_u16le(const uint8_t *b) {
    return (uint16_t)(b[0] | (b[1] << 8));
}

// ---- httpd ----

void httpd::init() {
    g_httpd = this;
    cyw43_arch_enable_sta_mode();
}

void httpd::connect() {
    connection_attempts++;
    if (log_enabled()) log("Connecting to Wi-Fi: %s, attempt %d", WIFI_SSID, connection_attempts);
    int result = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);
    if (result != 0) {
        string reason = (result == PICO_ERROR_TIMEOUT)       ? "timeout" :
                        (result == PICO_ERROR_BADAUTH)        ? "bad auth" :
                        (result == PICO_ERROR_CONNECT_FAILED) ? "conn failed" :
                        "unknown";
        if (log_enabled()) log("Wi-Fi failed: %d (%s)", result, reason.c_str());
        return;
    }
    ip4addr = ip4addr_ntoa(netif_ip4_addr(netif_list));
    if (log_enabled()) log("Wi-Fi connected, IP: %s", ip4addr.c_str());
    is_connected = true;
    start_time = get_absolute_time();
}

void httpd::start() {
    cyw43_arch_lwip_begin();

    // Serve static files (index.html) on port 80
    httpd_init();

    // WebSocket server on port 81
    ws.init(81);
    ws.on_message = [](const string& msg) {
        httpd& h = *httpd::g_httpd;

        // Send current state to a newly connected client
        if (msg == "__connected__") {
            h.notify();
            return;
        }

        if (msg.size() < 1) return;
        const uint8_t *b = (const uint8_t*)msg.data();
        uint8_t cmd = b[0];
        size_t  len = msg.size();

        if (log_enabled()) log("WS rx cmd=0x%02x len=%u", cmd, (unsigned)len);
        h.as.bt_centrals_json_array.clear();  // invalidate cache before notify

        switch (cmd) {
            case CMD_KBD_REPORT:
                if (len >= 9 && h.cmd_kbd_report)
                    h.cmd_kbd_report(b + 1);
                return;  // high-frequency, no state notify
            case CMD_MOUSE:
                if (len >= 5 && h.cmd_mouse_report)
                    h.cmd_mouse_report(b + 1);
                return;  // high-frequency, no state notify
            case CMD_BT_ADV_TOGGLE:
                if (h.cmd_bt_adv_toggle) h.cmd_bt_adv_toggle();
                break;
            case CMD_BT_CENTRAL_ACT:
                if (len >= 3 && h.cmd_bt_central_activate)
                    h.cmd_bt_central_activate(rd_u16le(b + 1));
                break;
            case CMD_BT_CENTRAL_UNPAIR:
                if (len >= 3 && h.cmd_bt_central_unpair)
                    h.cmd_bt_central_unpair(rd_u16le(b + 1));
                break;
            case CMD_TYPE: {
                if (len >= 3) {
                    uint16_t tlen = rd_u16le(b + 1);
                    if (log_enabled()) log("CMD_TYPE: msg_len=%u text_len=%u", (unsigned)len, (unsigned)tlen);
                    if (len >= (size_t)(3 + tlen) && h.cmd_type) {
                        string text((const char*)(b + 3), tlen);
                        if (log_enabled()) log("Sending text: %s", text.c_str());
                        h.cmd_type(text);
                    }
                }
                break;
            }
            case CMD_REBOOT:
                if (h.cmd_reboot) h.cmd_reboot();
                return;  // no notify after reboot
            default:
                if (log_enabled()) log("WS rx unknown cmd 0x%02x", cmd);
                return;
        }

        h.notify();
    };

    cyw43_arch_lwip_end();
}

void httpd::notify() {
    as.bt_centrals_json_array.clear();
    update_as_cache();
    uint64_t uptime_s = absolute_time_diff_us(start_time, get_absolute_time()) / 1000000ULL;
    string state =
        string("{\"uptime\":") + to_string(uptime_s) +
        ",\"bt_adv\":"  + (as.is_advertising ? "true" : "false") +
        ",\"ip\":\""    + ip4addr + "\"" +
        ",\"bt_devices\":" + as.bt_centrals_json_array + "}";
    ws.send(state);
}

void httpd::update_as_cache() {
    if (as.bt_centrals_json_array.empty()) {
        as.bt_centrals_json_array = "[";
        for (size_t i = 0; i < as.bt_centrals.size(); i++) {
            const app_bt_central& c = as.bt_centrals[i];
            string elem = "{";
            elem += "\"id\":"           + to_string(c.id);
            elem += ",\"name\":\""      + c.name + "\"";
            elem += ",\"is_active\":"   + string(c.is_active ? "true" : "false");
            elem += ",\"addr\":\""      + c.addr + "\"";
            elem += ",\"addr_type\":\"" + c.addr_type + "\"";
            elem += "}";
            as.bt_centrals_json_array += elem;
            if (i < as.bt_centrals.size() - 1)
                as.bt_centrals_json_array += ",";
        }
        as.bt_centrals_json_array += "]";
    }
}
