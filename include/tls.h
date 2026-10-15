// LemonOS Baremetal TLS 1.2 Client Engine (include/tls.h)
// Supports TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 (0xC02F) via X25519 Montgomery Ladder

#ifndef TLS_H
#define TLS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// TLS Session State
typedef struct {
    uint8_t  client_write_key[16];
    uint8_t  server_write_key[16];
    uint8_t  client_write_iv[4];
    uint8_t  server_write_iv[4];
    uint64_t client_seq;
    uint64_t server_seq;
    int      connected;
} tls_session_t;

// TLS 1.2 API
int  tls_connect(uint32_t ip, uint16_t port, const char* sni_host, tls_session_t* out_session);
int  tls_send(tls_session_t* session, const void* data, uint32_t len);
int  tls_recv(tls_session_t* session, void* buf, uint32_t max_len, uint32_t timeout_ms);
void tls_close(tls_session_t* session);

// HTTPS High-Level GET
int  net_https_get_full(const char* url, char* hdr_buf, uint32_t max_hdr, char* body_buf, uint32_t max_body, int* out_status);

#ifdef __cplusplus
}
#endif

#endif // TLS_H
