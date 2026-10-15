#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stddef.h>

// Byte order helpers
static inline uint16_t htons(uint16_t val) {
    return (uint16_t)((val << 8) | (val >> 8));
}
static inline uint16_t ntohs(uint16_t val) {
    return htons(val);
}
static inline uint32_t htonl(uint32_t val) {
    return ((val << 24) & 0xFF000000) |
           ((val << 8)  & 0x00FF0000) |
           ((val >> 8)  & 0x0000FF00) |
           ((val >> 24) & 0x000000FF);
}
static inline uint32_t ntohl(uint32_t val) {
    return htonl(val);
}

// IP address helper macro: IP(192, 168, 1, 1) -> uint32_t
#define MAKE_IP(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

// EtherTypes
#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP  0x0806

// IP Protocols
#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

// Ethernet Frame Header (14 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
} eth_header_t;

// ARP Packet (28 bytes)
typedef struct __attribute__((packed)) {
    uint16_t hw_type;      // 1 for Ethernet
    uint16_t proto_type;   // 0x0800 for IPv4
    uint8_t  hw_size;      // 6
    uint8_t  proto_size;   // 4
    uint16_t opcode;       // 1 for Request, 2 for Reply
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
} arp_packet_t;

// IPv4 Header (20 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  ver_ihl;      // Version (4 bits) + IHL (4 bits)
    uint8_t  tos;          // Type of Service
    uint16_t total_len;    // Total Length
    uint16_t id;           // Identification
    uint16_t flags_frag;   // Flags (3 bits) + Fragment Offset (13 bits)
    uint8_t  ttl;          // Time to Live
    uint8_t  protocol;     // 1=ICMP, 6=TCP, 17=UDP
    uint16_t checksum;     // Header Checksum
    uint32_t src_ip;       // Source IP
    uint32_t dest_ip;      // Destination IP
} ipv4_header_t;

// ICMP Header (8 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  type;         // 8=Echo Request, 0=Echo Reply
    uint8_t  code;         // 0
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} icmp_header_t;

// UDP Header (8 bytes)
typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} udp_header_t;

// TCP Header (20 bytes without options)
typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset; // 4 bits offset, 4 bits reserved
    uint8_t  flags;       // FIN=0x01, SYN=0x02, RST=0x04, PSH=0x08, ACK=0x10, URG=0x20
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} tcp_header_t;

// Network Configuration
typedef struct {
    uint8_t  mac[6];
    uint32_t ip;
    uint32_t subnet_mask;
    uint32_t gateway;
    uint32_t dns_server;
    int      dhcp_active;
} net_config_t;

// Core Network API
int  net_init(void);
void net_poll(void);
void net_get_config(net_config_t* cfg);
void net_set_ip(uint32_t ip, uint32_t subnet, uint32_t gateway, uint32_t dns);

// Protocol Senders
int  net_send_ethernet(const uint8_t* dest_mac, uint16_t ethertype, const void* payload, uint16_t len);
int  net_send_arp_request(uint32_t target_ip);
int  net_resolve_arp(uint32_t target_ip, uint8_t* out_mac);
int  net_send_ipv4(uint32_t dest_ip, uint8_t protocol, const void* payload, uint16_t len);
int  net_send_udp(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void* payload, uint16_t len);

// ICMP Ping
int  net_ping(uint32_t target_ip, int count, int timeout_ms);
int  net_ping_ex(uint32_t target_ip, int count, int timeout_ms, void (*out_line)(const char* line, uint32_t color));

// DHCP Client
int  net_dhcp_request(void);

// DNS Resolver
int  net_dns_resolve(const char* hostname, uint32_t* out_ip);

// HTTP Client (Download & Upload)
int  net_http_get(const char* url, char* out_buf, uint32_t max_len, int* out_status);
int  net_http_get_full(const char* url, char* hdr_buf, uint32_t max_hdr, char* body_buf, uint32_t max_body, int* out_status);
int  net_http_post(const char* url, const char* content_type, const void* data, uint32_t data_len, char* out_buf, uint32_t max_len, int* out_status);
int  net_download_file(const char* url, const char* local_path);
int  net_upload_file(const char* local_path, const char* url);

// TCP Socket API
int  net_tcp_connect(uint32_t ip, uint16_t port, uint32_t timeout_ms);
int  net_tcp_send(const void* data, uint32_t len);
int  net_tcp_recv(void* buf, uint32_t max_len, uint32_t timeout_ms);
int  net_tcp_is_closed(void);
void net_tcp_close(void);

// String conversions
void     net_ip_to_str(uint32_t ip, char* buf);
uint32_t net_str_to_ip(const char* str);
void     net_mac_to_str(const uint8_t* mac, char* buf);

// Automated Network Test Suite
void net_run_tests(void);

#endif
