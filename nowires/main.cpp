#include <stdio.h>
#include <string>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

// lwip
#include "lwip/ip4_addr.h"
#include "lwip/apps/mdns.h"
#include "lwip/init.h"
#include "lwip/apps/httpd.h"

using namespace std;

/**
 * I _prefer_ all of it in one single file.
 */

// flags
bool log_enabled = true;
// int line_id = 0;


// --- logging ---
void log(const char *format, ...) {
    if (!log_enabled) return;

    // convert line_id to string and print it, padding with spaces to 4 characters
    // printf("%d. ", line_id++);
    printf("log: ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

// --- dashboard ---

void led_put(bool on) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

void led_blink(int times, int delay_ms) {
    for (int i = 0; i < times; i++) {
        led_put(true);
        sleep_ms(delay_ms);
        led_put(false);
        sleep_ms(delay_ms);
    }
}

void led_blink_sos_forever() {
    while (true) {
        // S
        led_put(true);
        sleep_ms(200);
        led_put(false);
        sleep_ms(200);
        led_put(true);
        sleep_ms(200);
        led_put(false);
        sleep_ms(200);
        led_put(true);
        sleep_ms(200);
        led_put(false);
        sleep_ms(600);

        // O
        led_put(true);
        sleep_ms(600);
        led_put(false);
        sleep_ms(200);
        led_put(true);
        sleep_ms(600);
        led_put(false);
        sleep_ms(200);
        led_put(true);
        sleep_ms(600);
        led_put(false);
        sleep_ms(600);

        // S
        led_put(true);
        sleep_ms(200);
        led_put(false);
        sleep_ms(200);
        led_put(true);
        sleep_ms(200);
        led_put(false);
        sleep_ms(200);
        led_put(true);
        sleep_ms(200);
        led_put(false);
        sleep_ms(1000); // wait before repeating
    }
}

// --- http ---

static bool led_on = false;

static const char *cgi_handler_test(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
    if (iNumParams > 0) {
        if (strcmp(pcParam[0], "test") == 0) {
            return "/test.shtml";
        }
    }
    return "/index.shtml";
}

static tCGI cgi_handlers[] = {
    { "/", cgi_handler_test },
    { "/index.shtml", cgi_handler_test },
};

// Note that the buffer size is limited by LWIP_HTTPD_MAX_TAG_INSERT_LEN, so use LWIP_HTTPD_SSI_MULTIPART to return larger amounts of data
u16_t ssi_example_ssi_handler(int iIndex, char *pcInsert, int iInsertLen
#if LWIP_HTTPD_SSI_MULTIPART
    , uint16_t current_tag_part, uint16_t *next_tag_part
#endif
) {
    size_t printed;
    switch (iIndex) {
        case 0: { // "status"
            printed = snprintf(pcInsert, iInsertLen, "Pass");
            break;
        }
        case 1: { // "welcome"
            printed = snprintf(pcInsert, iInsertLen, "Hello from Pico");
            break;
        }
        case 2: { // "uptime"
            // uint64_t uptime_s = absolute_time_diff_us(wifi_connected_time, get_absolute_time()) / 1e6;
            // printed = snprintf(pcInsert, iInsertLen, "%"PRIu64, uptime_s);
            break;
        }
        case 3: { // "ledstate"
            printed = snprintf(pcInsert, iInsertLen, "%s", led_on ? "ON" : "OFF");
            break;
        }
        case 4: { // "ledinv"
            printed = snprintf(pcInsert, iInsertLen, "%s", led_on ? "OFF" : "ON");
            break;
        }
#if LWIP_HTTPD_SSI_MULTIPART
        case 5: { /* "table" */
            printed = snprintf(pcInsert, iInsertLen, "<tr><td>This is table row number %d</td></tr>", current_tag_part + 1);
            // Leave "next_tag_part" unchanged to indicate that all data has been returned for this tag
            if (current_tag_part < 9) {
                *next_tag_part = current_tag_part + 1;
            }
            break;
        }
#endif
        default: { // unknown tag
            printed = 0;
            break;
        }
    }
  return (u16_t)printed;
}

// Be aware of LWIP_HTTPD_MAX_TAG_NAME_LEN
static const char *ssi_tags[] = {
    "status",
    "welcome",
    "uptime",
    "ledstate",
    "ledinv",
    "table",
};

#if LWIP_HTTPD_SUPPORT_POST

#define LED_STATE_BUFSIZE 4
static void *current_connection;

err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
        u16_t http_request_len, int content_len, char *response_uri,
        u16_t response_uri_len, u8_t *post_auto_wnd) {
    if (memcmp(uri, "/led.cgi", 8) == 0 && current_connection != connection) {
        current_connection = connection;
        snprintf(response_uri, response_uri_len, "/ledfail.shtml");
        *post_auto_wnd = 1;
        return ERR_OK;
    }
    return ERR_VAL;
}

// Return a value for a parameter
char *httpd_param_value(struct pbuf *p, const char *param_name, char *value_buf, size_t value_buf_len) {
    size_t param_len = strlen(param_name);
    u16_t param_pos = pbuf_memfind(p, param_name, param_len, 0);
    if (param_pos != 0xFFFF) {
        u16_t param_value_pos = param_pos + param_len;
        u16_t param_value_len = 0;
        u16_t tmp = pbuf_memfind(p, "&", 1, param_value_pos);
        if (tmp != 0xFFFF) {
            param_value_len = tmp - param_value_pos;
        } else {
            param_value_len = p->tot_len - param_value_pos;
        }
        if (param_value_len > 0 && param_value_len < value_buf_len) {
            char *result = (char *)pbuf_get_contiguous(p, value_buf, value_buf_len, param_value_len, param_value_pos);
            if (result) {
                result[param_value_len] = 0;
                return result;
            }
        }
    }
    return NULL;
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
    err_t ret = ERR_VAL;
    LWIP_ASSERT("NULL pbuf", p != NULL);
    if (current_connection == connection) {
        char buf[LED_STATE_BUFSIZE];
        char *val = httpd_param_value(p, "led_state=", buf, sizeof(buf));
        if (val) {
            led_on = (strcmp(val, "ON") == 0) ? true : false;
            cyw43_gpio_set(&cyw43_state, 0, led_on);
            ret = ERR_OK;
        }
    }
    pbuf_free(p);
    return ret;
}

void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len) {
    snprintf(response_uri, response_uri_len, "/ledfail.shtml");
    if (current_connection == connection) {
        snprintf(response_uri, response_uri_len, "/ledpass.shtml");
    }
    current_connection = NULL;
}

#endif

class httpd {
public:
    bool is_connected{false};
    string ip4addr;
    absolute_time_t connected_time; // when the connection was established

    void init() {
        // Enable Wi-Fi station mode (regular Wi-Fi client)
        cyw43_arch_enable_sta_mode();

        // set to hardcoded hostname for now, as the get_mac_ascii function is not working
        // netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], "hydra");
    }

    void connect() {
        // Connect to Wi-Fi network
        log("Connecting to Wi-Fi network: %s/%s", WIFI_SSID, WIFI_PASSWORD);
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
        connected_time = get_absolute_time();
        is_connected = true;
    }

    void start() {
        // setup http server
        cyw43_arch_lwip_begin();
        httpd_init();
        http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));
        http_set_ssi_handler(ssi_example_ssi_handler, ssi_tags, LWIP_ARRAYSIZE(ssi_tags));
        cyw43_arch_lwip_end();
    }
};

// --- main entry point ---

int main() {
    stdio_init_all();

    log("Hydra [%s]", VTAG);

    // Initialise the Wi-Fi/bluetooth chip
    if (cyw43_arch_init()) {
        log("Wireless chip init failed\n");
        led_blink_sos_forever();
    }

    led_blink(5, 100);

    httpd h;
    h.init();
    h.connect();
    h.start();

    if(!h.is_connected) {
        led_blink_sos_forever();
        return -1;
    }

    while (true) {

#if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(led_time);
#else
        sleep_ms(1000);
        led_blink(1, 100);
#endif

        // log("Hydra [%s] %s:%s", VTAG, WIFI_SSID, WIFI_PASSWORD);

        // cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        // sleep_ms(500);
        // cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        // sleep_ms(500);
    }
}
