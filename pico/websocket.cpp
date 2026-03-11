#include "websocket.h"
#include "log.h"
#include "lwip/tcp.h"
#include <string.h>
#include <string>

// ---- Compact SHA-1 (for WebSocket handshake, RFC 6455) ----

#define ROL32(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

static void sha1(const uint8_t *in, size_t len, uint8_t out[20]) {
    uint32_t s[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};

    auto process_block = [&](const uint8_t *b) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)b[i*4] << 24) | ((uint32_t)b[i*4+1] << 16) |
                   ((uint32_t)b[i*4+2] << 8) | (uint32_t)b[i*4+3];
        for (int i = 16; i < 80; i++)
            w[i] = ROL32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

        uint32_t a = s[0], b_ = s[1], c = s[2], d = s[3], e = s[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if      (i < 20) { f = (b_ & c) | (~b_ & d); k = 0x5A827999u; }
            else if (i < 40) { f = b_ ^ c ^ d;            k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b_ & c) | (b_ & d) | (c & d); k = 0x8F1BBCDCu; }
            else             { f = b_ ^ c ^ d;            k = 0xCA62C1D6u; }
            uint32_t t = ROL32(a, 5) + f + e + k + w[i];
            e = d; d = c; c = ROL32(b_, 30); b_ = a; a = t;
        }
        s[0] += a; s[1] += b_; s[2] += c; s[3] += d; s[4] += e;
    };

    uint8_t buf[64];
    uint8_t pos = 0;
    for (size_t i = 0; i < len; i++) {
        buf[pos++] = in[i];
        if (pos == 64) { process_block(buf); pos = 0; }
    }
    buf[pos++] = 0x80;
    if (pos > 56) { while (pos < 64) buf[pos++] = 0; process_block(buf); pos = 0; }
    while (pos < 56) buf[pos++] = 0;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) buf[56 + i] = (uint8_t)(bits >> ((7 - i) * 8));
    process_block(buf);

    for (int i = 0; i < 5; i++) {
        out[i*4]   = s[i] >> 24; out[i*4+1] = s[i] >> 16;
        out[i*4+2] = s[i] >> 8;  out[i*4+3] = (uint8_t)s[i];
    }
}

// ---- Base64 encode ----

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64enc(const uint8_t *in, size_t n, char *out) {
    size_t i = 0, j = 0;
    while (i + 2 < n) {
        out[j++] = B64[in[i] >> 2];
        out[j++] = B64[((in[i] & 3) << 4) | (in[i+1] >> 4)];
        out[j++] = B64[((in[i+1] & 0xf) << 2) | (in[i+2] >> 6)];
        out[j++] = B64[in[i+2] & 0x3f];
        i += 3;
    }
    if (i < n) {
        out[j++] = B64[in[i] >> 2];
        if (i + 1 < n) { out[j++] = B64[((in[i] & 3) << 4) | (in[i+1] >> 4)]; out[j++] = B64[(in[i+1] & 0xf) << 2]; }
        else           { out[j++] = B64[(in[i] & 3) << 4]; out[j++] = '='; }
        out[j++] = '=';
    }
    out[j] = '\0';
}

// ---- ws_server ----

void ws_server::close_client() {
    if (!client_pcb_) return;
    log("WS: client disconnected");
    tcp_arg(client_pcb_, nullptr);
    tcp_recv(client_pcb_, nullptr);
    tcp_err(client_pcb_, nullptr);
    tcp_close(client_pcb_);
    client_pcb_ = nullptr;
    hs_done_ = false;
    recv_buf_.clear();
}

void ws_server::send_frame(struct tcp_pcb *pcb, const uint8_t *data, size_t len, uint8_t opcode) {
    uint8_t hdr[4];
    size_t hdr_len;
    hdr[0] = 0x80 | opcode;
    if (len < 126) {
        hdr[1] = (uint8_t)len;
        hdr_len = 2;
    } else {
        hdr[1] = 126;
        hdr[2] = (len >> 8) & 0xff;
        hdr[3] = len & 0xff;
        hdr_len = 4;
    }
    err_t err = tcp_write(pcb, hdr, (u16_t)hdr_len, TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
    if (err != ERR_OK) { log("WS: tx hdr err %d", (int)err); return; }
    err = tcp_write(pcb, data, (u16_t)len, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) { log("WS: tx data err %d", (int)err); return; }
    tcp_output(pcb);
}

void ws_server::send(const std::string& data) {
    if (!client_pcb_ || !hs_done_) return;
    send_frame(client_pcb_, (const uint8_t*)data.data(), data.size());
}

void ws_server::handle_data(struct tcp_pcb *pcb, const char *data, uint16_t len) {
    recv_buf_.append(data, len);

    if (!hs_done_) {
        // Wait for end of HTTP headers
        auto hdr_end = recv_buf_.find("\r\n\r\n");
        if (hdr_end == std::string::npos) return;

        // Extract Sec-WebSocket-Key
        const char *key_hdr = "Sec-WebSocket-Key: ";
        auto kpos = recv_buf_.find(key_hdr);
        if (kpos == std::string::npos) { close_client(); return; }
        kpos += strlen(key_hdr);
        auto kend = recv_buf_.find("\r\n", kpos);
        if (kend == std::string::npos) { close_client(); return; }
        std::string key = recv_buf_.substr(kpos, kend - kpos);
        log("WS: key=%s", key.c_str());

        // Compute Sec-WebSocket-Accept = base64(SHA1(key + GUID))
        std::string concat = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        uint8_t sha[20]; char accept[32];
        ::sha1((const uint8_t*)concat.data(), concat.size(), sha);
        ::b64enc(sha, 20, accept);

        char resp[256];
        int rlen = snprintf(resp, sizeof(resp),
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n\r\n", accept);
        tcp_write(pcb, resp, (u16_t)rlen, TCP_WRITE_FLAG_COPY);
        tcp_output(pcb);

        hs_done_ = true;
        recv_buf_ = recv_buf_.substr(hdr_end + 4);
        log("WS: connected");
        if (on_message) on_message("__connected__");
        return;
    }

    // Parse WebSocket frames (client frames are always masked, RFC 6455 §5.3)
    while (recv_buf_.size() >= 2) {
        const uint8_t *b = (const uint8_t*)recv_buf_.data();
        uint8_t opcode = b[0] & 0x0f;
        bool    masked = (b[1] & 0x80) != 0;
        size_t  plen   = b[1] & 0x7f;
        size_t  offset = 2;

        if (plen == 126) {
            if (recv_buf_.size() < 4) return;
            plen   = ((size_t)b[2] << 8) | b[3];
            offset = 4;
        } else if (plen == 127) {
            // 64-bit length not needed for our use case
            close_client(); return;
        }

        size_t frame_size = offset + (masked ? 4u : 0u) + plen;
        if (recv_buf_.size() < frame_size) return;  // wait for more data

        if (opcode == 0x08) { close_client(); return; }  // close frame

        if (opcode == 0x09) {  // ping -> pong
            send_frame(pcb, b + offset + (masked ? 4u : 0u), plen, 0x0a);
        } else if (opcode == 0x01 || opcode == 0x02) {  // text / binary
            std::string payload;
            if (masked) {
                const uint8_t *mask  = b + offset;
                const uint8_t *pdata = b + offset + 4;
                payload.resize(plen);
                for (size_t i = 0; i < plen; i++) payload[i] = pdata[i] ^ mask[i & 3];
            } else {
                payload.assign((const char*)(b + offset), plen);
            }
            if (on_message) on_message(payload);
        }

        recv_buf_.erase(0, frame_size);
    }
}

err_t ws_server::on_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    ws_server *self = (ws_server*)arg;
    if (!p) { self->close_client(); return ERR_OK; }
    for (struct pbuf *q = p; q; q = q->next)
        self->handle_data(tpcb, (const char*)q->payload, q->len);
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

void ws_server::on_err(void *arg, err_t err) {
    ws_server *self = (ws_server*)arg;
    log("WS: error %d", (int)err);
    // tcp_pcb is already freed by lwIP at this point, do not call tcp_close
    self->client_pcb_ = nullptr;
    self->hs_done_ = false;
    self->recv_buf_.clear();
}

err_t ws_server::on_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    if (err != ERR_OK || !newpcb) return ERR_VAL;
    ws_server *self = (ws_server*)arg;
    if (self->client_pcb_) self->close_client();  // drop previous client
    log("WS: new connection");
    self->client_pcb_ = newpcb;
    self->hs_done_ = false;
    self->recv_buf_.clear();
    tcp_arg(newpcb, self);
    tcp_recv(newpcb, on_recv);
    tcp_err(newpcb, on_err);
    tcp_setprio(newpcb, TCP_PRIO_MIN);
    return ERR_OK;
}

void ws_server::init(uint16_t port) {
    struct tcp_pcb *pcb = tcp_new();
    LWIP_ASSERT("ws tcp_new", pcb != nullptr);
    tcp_bind(pcb, IP_ADDR_ANY, port);
    listen_pcb_ = tcp_listen(pcb);
    tcp_arg(listen_pcb_, this);
    tcp_accept(listen_pcb_, on_accept);
    log("WS: listening on port %u", port);
}
