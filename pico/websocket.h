#pragma once
#include "lwip/tcp.h"
#include <functional>
#include <string>

// Minimal WebSocket server using lwIP raw TCP API.
// Supports a single client connection at a time.
// Must call send() from within the lwIP context (e.g. inside a TCP callback
// or between cyw43_arch_lwip_begin() / cyw43_arch_lwip_end()).
class ws_server {
public:
    // Called when a text/binary frame is received, or "__connected__" on connect.
    std::function<void(const std::string&)> on_message;

    void init(uint16_t port);
    void send(const std::string& data);

private:
    struct tcp_pcb *listen_pcb_{nullptr};
    struct tcp_pcb *client_pcb_{nullptr};
    bool hs_done_{false};
    std::string recv_buf_;

    static err_t on_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
    static err_t on_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
    static void  on_err(void *arg, err_t err);

    void close_client();
    void handle_data(struct tcp_pcb *pcb, const char *data, uint16_t len);
    void send_frame(struct tcp_pcb *pcb, const uint8_t *data, size_t len, uint8_t opcode = 0x01);
};
