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

const char *cgi_handler_root(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
    return "/index.shtml";
}

tCGI cgi_handlers[] = {
    { "/", cgi_handler_root },
};

// Note that the buffer size is limited by LWIP_HTTPD_MAX_TAG_INSERT_LEN, so use LWIP_HTTPD_SSI_MULTIPART to return larger amounts of data
u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen
#if LWIP_HTTPD_SSI_MULTIPART
    , uint16_t current_tag_part, uint16_t *next_tag_part
#endif
) {
    size_t printed;
    httpd& h = *httpd::g_httpd;
    switch (iIndex) {
        case 0: { // "uptime"
            uint64_t uptime_s = absolute_time_diff_us(h.start_time, get_absolute_time()) / 1e6;
            printed = snprintf(pcInsert, iInsertLen, "%" PRIu64, uptime_s);
            break;
        }
        case 1: { // "btdevs"
            h.update_as_cache();
            printed = snprintf(pcInsert, iInsertLen, "%s", h.as.bt_centrals_json_array.c_str());
            break;
        }
        case 2: { // "btadv"
            printed = snprintf(pcInsert, iInsertLen, "%s", h.as.is_advertising ? "true" : "false");
            break;
        }
        case 3: { // "ip"
            printed = snprintf(pcInsert, iInsertLen, "%s", h.ip4addr.c_str());
            break;
        }
        default: {
            printed = 0;
            break;
        }
    }
    return (u16_t)printed;
}

// tag names (max LWIP_HTTPD_MAX_TAG_NAME_LEN chars, default 8)
static const char *ssi_tags[] = {
    "uptime",   // 0
    "btdevs",   // 1
    "btadv",    // 2
    "ip",       // 3
};

#if LWIP_HTTPD_SUPPORT_POST

#define POST_BUF_SIZE 32

err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
        u16_t http_request_len, int content_len, char *response_uri,
        u16_t response_uri_len, u8_t *post_auto_wnd) {

    log("-> POST %s", uri);
    // accept POSTs to /cmd
    if (strcmp(uri, "/cmd") == 0) {
        snprintf(response_uri, response_uri_len, "/ok.shtml");
        *post_auto_wnd = 1;
        return ERR_OK;
    }
    return ERR_VAL;
}

// Extract value for "name=value" from form-urlencoded pbuf
static char *post_param(struct pbuf *p, const char *name, char *buf, size_t buf_len) {
    size_t name_len = strlen(name);
    u16_t pos = pbuf_memfind(p, name, name_len, 0);
    if (pos == 0xFFFF) return NULL;
    u16_t val_pos = pos + name_len;
    u16_t end = pbuf_memfind(p, "&", 1, val_pos);
    u16_t val_len = (end != 0xFFFF) ? (end - val_pos) : (p->tot_len - val_pos);
    if (val_len == 0 || val_len >= buf_len) return NULL;
    char *result = (char *)pbuf_get_contiguous(p, buf, buf_len, val_len, val_pos);
    if (result) result[val_len] = 0;
    return result;
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
    LWIP_ASSERT("NULL pbuf", p != NULL);
    err_t ret = ERR_VAL;
    httpd& h = *httpd::g_httpd;

    char buf[POST_BUF_SIZE];
    string action = post_param(p, "action=", buf, sizeof(buf));
    log("Received POST data: action=%s", action.c_str());

    if(action == "bt_adv_toggle") {
        if(h.cmd_bt_adv_toggle) {
            h.cmd_bt_adv_toggle();
            ret = ERR_OK;
        }
    } else if(action == "bt_central_activate") {
        string id_str = post_param(p, "value=", buf, sizeof(buf));
        log("Central ID to activate: %s", id_str.c_str());
        if(!id_str.empty()) {
            uint16_t central_id = (uint16_t)strtoul(id_str.c_str(), NULL, 10);
            if(h.cmd_bt_central_activate) {
                h.cmd_bt_central_activate(central_id);
                ret = ERR_OK;
            }
        }
    } else if(action == "type") {
        string text = post_param(p, "value=", buf, sizeof(buf));
        log("Received text input: %s", text.c_str());
        if(!text.empty() && h.cmd_type) {
            h.cmd_type(text);
            ret = ERR_OK;
        }
    } else if(action == "reboot") {
        if(h.cmd_reboot) {
            h.cmd_reboot();
            ret = ERR_OK;
        }
    }

    pbuf_free(p);
    return ret;
}

void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len) {
    snprintf(response_uri, response_uri_len, "/ok.shtml");
    log("<- POST done");
}

#endif

void httpd::init() {
    g_httpd = this;
    // Enable Wi-Fi station mode (regular Wi-Fi client)
    cyw43_arch_enable_sta_mode();

    // set to hardcoded hostname for now, as the get_mac_ascii function is not working
    // netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], "hydra");
}

void httpd::connect()  {
    // Connect to Wi-Fi network
    connection_attempts++;
    log("Connecting to Wi-Fi network: %s/%s, attempt: %d", WIFI_SSID, WIFI_PASSWORD, connection_attempts);
    int result = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);
    if (result != 0) {
        // 0 if the connection is successful.
        // PICO_ERROR_TIMEOUT: timeout is reached before a successful connection.
        // PICO_ERROR_BADAUTH: WiFi password is wrong.
        // PICO_ERROR_CONNECT_FAILED: connection failed for some other reason.
        string reason = (result == PICO_ERROR_TIMEOUT) ? "timeout" :
                        (result == PICO_ERROR_BADAUTH) ? "bad authentication" :
                        (result == PICO_ERROR_CONNECT_FAILED) ? "connection failed" :
                        "unknown error";
        log("Wi-Fi connection failed: %d (%s)", result, reason.c_str());
        return;
    }

    ip4addr = ip4addr_ntoa(netif_ip4_addr(netif_list));
    log("Connected to Wi-Fi, IP address: %s", ip4addr.c_str());
    is_connected = true;
    start_time = get_absolute_time();
}

void httpd::start() {
    // setup http server
    cyw43_arch_lwip_begin();
    httpd_init();
    http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));
    http_set_ssi_handler(ssi_handler, ssi_tags, LWIP_ARRAYSIZE(ssi_tags));
    cyw43_arch_lwip_end();
}

void httpd::update_as_cache() {
    if(as.bt_centrals_json_array.empty()) {
        as.bt_centrals_json_array = "[";
        for(size_t i = 0; i < as.bt_centrals.size(); i++) {
            const app_bt_central& c = as.bt_centrals[i];
            as.bt_centrals_json_array += "{\"id\":" + to_string(c.id) + ",\"is_active\":" + (c.is_active ? "true" : "false") + ",\"addr\":\"" + c.addr + "\",\"addr_type\":\"" + c.addr_type + "\"}";
            if(i < as.bt_centrals.size() - 1) {
                as.bt_centrals_json_array += ",";
            }
        }
        as.bt_centrals_json_array += "]";
    }
}
