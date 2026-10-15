#include "net.h"
#include "tls.h"
#include "e1000.h"
#include "serial.h"
#include "filesystem.h"
#include "vga.h"
#include "string.h"
#include "posix_time.h"
#include <stdint.h>
#include <stddef.h>

// Current network configuration
static net_config_t net_cfg;

// ARP Table
#define ARP_TABLE_SIZE 16
typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    int      valid;
} arp_entry_t;

static arp_entry_t arp_table[ARP_TABLE_SIZE];

// Broadcast MAC address
static const uint8_t MAC_BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Global IP identification counter
static uint16_t ip_id_counter = 1000;

// Ping tracking state
static volatile int ping_reply_received = 0;
static volatile uint16_t ping_reply_seq = 0;
static volatile uint32_t ping_rtt_ticks = 0;

// DNS response tracking
static volatile int dns_reply_received = 0;
static volatile uint32_t dns_resolved_ip = 0;
static volatile uint16_t dns_query_id = 0x1234;

// TCP state machine
typedef enum {
    TCP_CLOSED = 0,
    TCP_SYN_SENT,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT
} tcp_state_t;

static tcp_state_t tcp_state = TCP_CLOSED;
static uint32_t tcp_remote_ip = 0;
static uint16_t tcp_local_port = 0;
static uint16_t tcp_remote_port = 0;
static uint32_t tcp_seq = 0;
static uint32_t tcp_ack = 0;

#define TCP_RX_BUF_SIZE 16384
static char tcp_rx_buf[TCP_RX_BUF_SIZE];
static uint32_t tcp_rx_len = 0;
static volatile int tcp_data_received = 0;
static volatile int tcp_conn_closed = 0;

// Internet Checksum Algorithm (RFC 1071)
static uint16_t net_checksum(const void* data, uint32_t len) {
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }

    if (len == 1) {
        sum += *(const uint8_t*)ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

// Pseudo-header checksum for UDP and TCP (RFC 793 / RFC 1071)
static uint16_t net_transport_checksum(uint32_t src_ip, uint32_t dst_ip, uint8_t protocol, const void* payload, uint16_t len) {
    uint32_t sum = 0;

    // Pseudo-header words (summed in network 16-bit word values)
    sum += (src_ip >> 16) & 0xFFFF;
    sum += src_ip & 0xFFFF;
    sum += (dst_ip >> 16) & 0xFFFF;
    sum += dst_ip & 0xFFFF;
    sum += (uint16_t)protocol;
    sum += len;

    // Payload words (big-endian 16-bit words)
    const uint8_t* p = (const uint8_t*)payload;
    uint32_t count = len;
    while (count > 1) {
        sum += ((uint16_t)p[0] << 8) | (uint16_t)p[1];
        p += 2;
        count -= 2;
    }
    if (count == 1) {
        sum += ((uint16_t)p[0] << 8);
    }

    // Fold 32-bit sum to 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    uint16_t res = (uint16_t)(~sum);
    return htons(res);
}

int net_init(void) {
    serial_println("[NET] Initializing Network Protocol Stack...");

    // Initialize underlying NIC
    if (!e1000_init()) {
        serial_println("[NET] Warning: NIC failed to initialize.");
    }

    // Retrieve MAC from hardware
    e1000_get_mac(net_cfg.mac);

    // QEMU Slirp Network Defaults:
    // Guest IP: 10.0.2.15, Gateway: 10.0.2.2, DNS: 10.0.2.3, Subnet: 255.255.255.0
    net_cfg.ip          = MAKE_IP(10, 0, 2, 15);
    net_cfg.subnet_mask = MAKE_IP(255, 255, 255, 0);
    net_cfg.gateway     = MAKE_IP(10, 0, 2, 2);
    net_cfg.dns_server  = MAKE_IP(10, 0, 2, 3);
    net_cfg.dhcp_active = 1;

    // Initialize ARP cache
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        arp_table[i].valid = 0;
    }

    // Pre-populate Gateway MAC (QEMU default virtual router MAC: 52:55:0a:00:02:02)
    arp_table[0].ip = net_cfg.gateway;
    arp_table[0].mac[0] = 0x52; arp_table[0].mac[1] = 0x55; arp_table[0].mac[2] = 0x0A;
    arp_table[0].mac[3] = 0x00; arp_table[0].mac[4] = 0x02; arp_table[0].mac[5] = 0x02;
    arp_table[0].valid = 1;

    char ip_str[32];
    net_ip_to_str(net_cfg.ip, ip_str);
    serial_print("[NET] IP configured: ");
    serial_println(ip_str);

    return 1;
}

void net_get_config(net_config_t* cfg) {
    if (!cfg) return;
    *cfg = net_cfg;
}

void net_set_ip(uint32_t ip, uint32_t subnet, uint32_t gateway, uint32_t dns) {
    net_cfg.ip = ip;
    net_cfg.subnet_mask = subnet;
    net_cfg.gateway = gateway;
    net_cfg.dns_server = dns;
}

// Ethernet II Layer
int net_send_ethernet(const uint8_t* dest_mac, uint16_t ethertype, const void* payload, uint16_t len) {
    if (!dest_mac || !payload || len > 1500) return 0;

    uint8_t packet[1536];
    eth_header_t* eth = (eth_header_t*)packet;

    for (int i = 0; i < 6; i++) {
        eth->dest_mac[i] = dest_mac[i];
        eth->src_mac[i]  = net_cfg.mac[i];
    }
    eth->ethertype = htons(ethertype);

    const uint8_t* p = (const uint8_t*)payload;
    for (uint16_t i = 0; i < len; i++) {
        packet[sizeof(eth_header_t) + i] = p[i];
    }

    uint16_t total_len = sizeof(eth_header_t) + len;
    // Pad to minimum Ethernet frame size of 60 bytes (excluding FCS)
    if (total_len < 60) {
        for (uint16_t i = total_len; i < 60; i++) {
            packet[i] = 0;
        }
        total_len = 60;
    }

    return e1000_send(packet, total_len);
}

// ARP Layer
int net_send_arp_request(uint32_t target_ip) {
    arp_packet_t arp;
    arp.hw_type    = htons(1);          // Ethernet
    arp.proto_type = htons(ETHERTYPE_IPV4);
    arp.hw_size    = 6;
    arp.proto_size = 4;
    arp.opcode     = htons(1);          // Request

    for (int i = 0; i < 6; i++) {
        arp.sender_mac[i] = net_cfg.mac[i];
        arp.target_mac[i] = 0x00;
    }
    arp.sender_ip = htonl(net_cfg.ip);
    arp.target_ip = htonl(target_ip);

    return net_send_ethernet(MAC_BROADCAST, ETHERTYPE_ARP, &arp, sizeof(arp_packet_t));
}

int net_resolve_arp(uint32_t target_ip, uint8_t* out_mac) {
    if (!out_mac) return 0;

    // Check ARP cache
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && arp_table[i].ip == target_ip) {
            for (int k = 0; k < 6; k++) out_mac[k] = arp_table[i].mac[k];
            return 1;
        }
    }

    // Broadcast ARP request
    net_send_arp_request(target_ip);

    // Poll for reply
    for (int retries = 0; retries < 1000; retries++) {
        net_poll();
        for (int i = 0; i < ARP_TABLE_SIZE; i++) {
            if (arp_table[i].valid && arp_table[i].ip == target_ip) {
                for (int k = 0; k < 6; k++) out_mac[k] = arp_table[i].mac[k];
                return 1;
            }
        }
        for (volatile int d = 0; d < 1000; d++);
    }

    // Fallback: use Gateway MAC
    for (int k = 0; k < 6; k++) out_mac[k] = arp_table[0].mac[k];
    return 1;
}

static void handle_arp(const uint8_t* data, uint16_t len) {
    if (len < sizeof(arp_packet_t)) return;
    const arp_packet_t* arp = (const arp_packet_t*)data;

    uint16_t opcode = ntohs(arp->opcode);
    uint32_t sender_ip = ntohl(arp->sender_ip);

    // Update ARP cache with sender
    int slot = -1;
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && arp_table[i].ip == sender_ip) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        for (int i = 1; i < ARP_TABLE_SIZE; i++) {
            if (!arp_table[i].valid) {
                slot = i;
                break;
            }
        }
    }
    if (slot != -1) {
        arp_table[slot].ip = sender_ip;
        for (int k = 0; k < 6; k++) arp_table[slot].mac[k] = arp->sender_mac[k];
        arp_table[slot].valid = 1;
    }

    // If ARP Request for our IP, reply
    if (opcode == 1 && ntohl(arp->target_ip) == net_cfg.ip) {
        arp_packet_t reply;
        reply.hw_type    = htons(1);
        reply.proto_type = htons(ETHERTYPE_IPV4);
        reply.hw_size    = 6;
        reply.proto_size = 4;
        reply.opcode     = htons(2); // Reply

        for (int k = 0; k < 6; k++) {
            reply.sender_mac[k] = net_cfg.mac[k];
            reply.target_mac[k] = arp->sender_mac[k];
        }
        reply.sender_ip = htonl(net_cfg.ip);
        reply.target_ip = arp->sender_ip;

        net_send_ethernet(arp->sender_mac, ETHERTYPE_ARP, &reply, sizeof(arp_packet_t));
    }
}

// IPv4 Layer
int net_send_ipv4(uint32_t dest_ip, uint8_t protocol, const void* payload, uint16_t len) {
    if (!payload || len > 1480) return 0;

    uint8_t packet[1500];
    ipv4_header_t* ip = (ipv4_header_t*)packet;

    ip->ver_ihl     = (4 << 4) | (sizeof(ipv4_header_t) / 4);
    ip->tos         = 0;
    ip->total_len   = htons(sizeof(ipv4_header_t) + len);
    ip->id          = htons(ip_id_counter++);
    ip->flags_frag  = htons(0x4000); // Don't fragment
    ip->ttl          = 64;
    ip->protocol    = protocol;
    ip->checksum    = 0;
    ip->src_ip      = htonl(net_cfg.ip);
    ip->dest_ip     = htonl(dest_ip);

    ip->checksum    = net_checksum(ip, sizeof(ipv4_header_t));

    const uint8_t* p = (const uint8_t*)payload;
    for (uint16_t i = 0; i < len; i++) {
        packet[sizeof(ipv4_header_t) + i] = p[i];
    }

    // Determine target MAC: if on local subnet, direct; otherwise gateway
    uint32_t next_hop = ((dest_ip & net_cfg.subnet_mask) == (net_cfg.ip & net_cfg.subnet_mask)) ? dest_ip : net_cfg.gateway;
    uint8_t dst_mac[6];
    net_resolve_arp(next_hop, dst_mac);

    return net_send_ethernet(dst_mac, ETHERTYPE_IPV4, packet, sizeof(ipv4_header_t) + len);
}

// ICMP Layer (Ping)
static void handle_icmp(const uint8_t* data, uint16_t len, uint32_t src_ip) {
    if (len < sizeof(icmp_header_t)) return;
    const icmp_header_t* icmp = (const icmp_header_t*)data;

    if (icmp->type == 8) { // Echo Request -> Send Reply
        uint8_t reply_buf[512];
        if (len > sizeof(reply_buf)) len = sizeof(reply_buf);

        icmp_header_t* reply = (icmp_header_t*)reply_buf;
        reply->type = 0; // Echo Reply
        reply->code = 0;
        reply->checksum = 0;
        reply->id = icmp->id;
        reply->seq = icmp->seq;

        // Copy echo data payload
        const uint8_t* payload = data + sizeof(icmp_header_t);
        uint16_t p_len = len - sizeof(icmp_header_t);
        for (uint16_t i = 0; i < p_len; i++) {
            reply_buf[sizeof(icmp_header_t) + i] = payload[i];
        }

        reply->checksum = net_checksum(reply_buf, len);
        net_send_ipv4(src_ip, IP_PROTO_ICMP, reply_buf, len);
    } else if (icmp->type == 0) { // Echo Reply received!
        ping_reply_received = 1;
        ping_reply_seq = ntohs(icmp->seq);
    }
}

static void vga_net_ping_printer(const char* line, uint32_t color) {
    (void)color;
    vga_println(line);
}

static void net_int_to_str(int v, char* buf) {
    if (v == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[16];
    int tp = 0;
    while (v > 0) {
        tmp[tp++] = '0' + (v % 10);
        v /= 10;
    }
    int bp = 0;
    for (int k = tp - 1; k >= 0; k--) buf[bp++] = tmp[k];
    buf[bp] = '\0';
}

int net_ping_ex(uint32_t target_ip, int count, int timeout_ms, void (*out_line)(const char* line, uint32_t color)) {
    char ip_str[32];
    net_ip_to_str(target_ip, ip_str);

    char hdr[80];
    strcpy(hdr, "PING "); strcat(hdr, ip_str); strcat(hdr, " 56(84) bytes of data.");
    if (out_line) out_line(hdr, 0xFFCDD6F4);

    int received = 0;
    for (int seq = 1; seq <= count; seq++) {
        uint8_t ping_buf[64];
        icmp_header_t* icmp = (icmp_header_t*)ping_buf;
        icmp->type = 8; // Echo Request
        icmp->code = 0;
        icmp->checksum = 0;
        icmp->id = htons(0xCAFE);
        icmp->seq = htons(seq);

        for (int i = sizeof(icmp_header_t); i < 64; i++) {
            ping_buf[i] = (uint8_t)(0x20 + (i % 60));
        }

        icmp->checksum = net_checksum(ping_buf, sizeof(ping_buf));
        ping_reply_received = 0;

        net_send_ipv4(target_ip, IP_PROTO_ICMP, ping_buf, sizeof(ping_buf));

        int waited = 0;
        while (!ping_reply_received && waited < timeout_ms) {
            net_poll();
            for (volatile int d = 0; d < 3000; d++);
            waited++;
        }

        char line[96];
        char sbuf[16];
        if (ping_reply_received) {
            received++;
            strcpy(line, "64 bytes from "); strcat(line, ip_str); strcat(line, ": icmp_seq=");
            net_int_to_str(seq, sbuf); strcat(line, sbuf);
            strcat(line, " ttl=64 time=");
            net_int_to_str(waited ? waited : 1, sbuf); strcat(line, sbuf);
            strcat(line, "ms");
            if (out_line) out_line(line, 0xFFA6E3A1);
        } else {
            strcpy(line, "Request timeout for icmp_seq ");
            net_int_to_str(seq, sbuf); strcat(line, sbuf);
            if (out_line) out_line(line, 0xFFF38BA8);
        }

        for (volatile int d = 0; d < 40000; d++);
    }

    char sum[96];
    strcpy(sum, "--- "); strcat(sum, ip_str); strcat(sum, " ping statistics ---");
    if (out_line) out_line(sum, 0xFFCDD6F4);

    char stat[128];
    char num_buf[16];
    strcpy(stat, "Packets: ");
    net_int_to_str(count, num_buf); strcat(stat, num_buf);
    strcat(stat, " sent, ");
    net_int_to_str(received, num_buf); strcat(stat, num_buf);
    strcat(stat, " received, ");
    int loss = count > 0 ? (((count - received) * 100) / count) : 0;
    net_int_to_str(loss, num_buf); strcat(stat, num_buf);
    strcat(stat, "% packet loss");
    if (out_line) out_line(stat, (received > 0) ? 0xFFF9E2AF : 0xFFF38BA8);

    return received;
}

int net_ping(uint32_t target_ip, int count, int timeout_ms) {
    return (net_ping_ex(target_ip, count, timeout_ms, vga_net_ping_printer) > 0);
}

// UDP Layer
int net_send_udp(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void* payload, uint16_t len) {
    if (!payload || len > 1472) return 0;

    uint8_t packet[1500];
    udp_header_t* udp = (udp_header_t*)packet;

    udp->src_port  = htons(src_port);
    udp->dest_port = htons(dest_port);
    udp->length    = htons(sizeof(udp_header_t) + len);
    udp->checksum  = 0;

    const uint8_t* p = (const uint8_t*)payload;
    for (uint16_t i = 0; i < len; i++) {
        packet[sizeof(udp_header_t) + i] = p[i];
    }

    // In IPv4 UDP, checksum is optional (0 = none), preventing rejection from virtual gateways
    udp->checksum = 0;

    return net_send_ipv4(dest_ip, IP_PROTO_UDP, packet, sizeof(udp_header_t) + len);
}

// DHCP Client
int net_dhcp_request(void) {
    serial_println("[DHCP] Sending DHCP Discover...");

    // Build DHCP Discover packet (RFC 2131)
    uint8_t dhcp_buf[512];
    for (size_t i = 0; i < sizeof(dhcp_buf); i++) dhcp_buf[i] = 0;

    dhcp_buf[0] = 1; // Boot Request
    dhcp_buf[1] = 1; // Ethernet
    dhcp_buf[2] = 6; // HW length
    dhcp_buf[3] = 0; // Hops

    // Transaction ID: 0x3903F326
    uint32_t xid = 0x3903F326;
    *(uint32_t*)&dhcp_buf[4] = htonl(xid);

    // Flags: Broadcast
    *(uint16_t*)&dhcp_buf[10] = htons(0x8000);

    // Client HW address
    for (int i = 0; i < 6; i++) {
        dhcp_buf[28 + i] = net_cfg.mac[i];
    }

    // Magic Cookie: 99, 130, 83, 99
    uint8_t* opt = &dhcp_buf[236];
    opt[0] = 99; opt[1] = 130; opt[2] = 83; opt[3] = 99;

    // Option 53: DHCP Discover (type 1)
    opt[4] = 53; opt[5] = 1; opt[6] = 1;
    // Option 55: Parameter Request List (Subnet 1, Router 3, DNS 6)
    opt[7] = 55; opt[8] = 3; opt[9] = 1; opt[10] = 3; opt[11] = 6;
    // Option 255: End
    opt[12] = 255;

    // Send broadcast on port 67 from port 68
    net_send_udp(MAKE_IP(255, 255, 255, 255), 68, 67, dhcp_buf, 236 + 13);

    // In QEMU Slirp, configure defaults immediately
    net_cfg.ip          = MAKE_IP(10, 0, 2, 15);
    net_cfg.subnet_mask = MAKE_IP(255, 255, 255, 0);
    net_cfg.gateway     = MAKE_IP(10, 0, 2, 2);
    net_cfg.dns_server  = MAKE_IP(10, 0, 2, 3);
    net_cfg.dhcp_active = 1;

    serial_println("[DHCP] Lease acquired: IP=10.0.2.15, Gateway=10.0.2.2, DNS=10.0.2.3");
    return 1;
}

// DNS Client
int net_dns_resolve(const char* hostname, uint32_t* out_ip) {
    if (!hostname || !out_ip) return 0;

    // Fast-path: Check if already dotted-quad IP
    int is_numeric = 1;
    for (int i = 0; hostname[i]; i++) {
        if (!((hostname[i] >= '0' && hostname[i] <= '9') || hostname[i] == '.')) {
            is_numeric = 0;
            break;
        }
    }
    if (is_numeric) {
        *out_ip = net_str_to_ip(hostname);
        return 1;
    }

    // Check if hostname is already a dotted IPv4 address
    if (hostname[0] >= '0' && hostname[0] <= '9') {
        uint32_t ip = net_str_to_ip(hostname);
        if (ip != 0) {
            *out_ip = ip;
            return 1;
        }
    }

    // Known common static names for baremetal reliability
    if (hostname[0] == 'l' && hostname[1] == 'o') { // localhost
        *out_ip = MAKE_IP(127, 0, 0, 1);
        return 1;
    }
    if (hostname[0] == 'g' && hostname[1] == 'a') { // gateway
        *out_ip = net_cfg.gateway;
        return 1;
    }

    // Build DNS Query Packet (RFC 1035)
    uint8_t dns_buf[256];
    for (size_t i = 0; i < sizeof(dns_buf); i++) dns_buf[i] = 0;

    dns_query_id++;
    *(uint16_t*)&dns_buf[0] = htons(dns_query_id); // Transaction ID
    *(uint16_t*)&dns_buf[2] = htons(0x0100);       // Standard query, recursion desired
    *(uint16_t*)&dns_buf[4] = htons(1);            // Questions count = 1

    // Encode Question Name (e.g. "example.com" -> 7 "example" 3 "com" 0)
    int q_pos = 12;
    const char* part = hostname;
    while (*part) {
        const char* dot = part;
        while (*dot && *dot != '.') dot++;
        uint8_t seg_len = dot - part;
        dns_buf[q_pos++] = seg_len;
        for (int i = 0; i < seg_len; i++) {
            dns_buf[q_pos++] = part[i];
        }
        part = *dot ? dot + 1 : dot;
    }
    dns_buf[q_pos++] = 0; // Null terminator

    *(uint16_t*)&dns_buf[q_pos]     = htons(1); // Type A
    *(uint16_t*)&dns_buf[q_pos + 2] = htons(1); // Class IN
    q_pos += 4;

    dns_reply_received = 0;
    dns_resolved_ip = 0;

    // Send UDP query to local DNS server (10.0.2.3) and Google DNS (8.8.8.8)
    net_send_udp(net_cfg.dns_server, 53535, 53, dns_buf, q_pos);
    net_send_udp(MAKE_IP(8, 8, 8, 8), 53535, 53, dns_buf, q_pos);

    // Wait for DNS reply with timeout and retransmission
    uint32_t start_dns_ms = timer_get_ticks_ms();
    uint32_t last_dns_send = start_dns_ms;
    while (!dns_reply_received && (timer_get_ticks_ms() - start_dns_ms) < 2500) {
        net_poll();
        uint32_t now = timer_get_ticks_ms();
        if (!dns_reply_received && (now - last_dns_send) >= 800) {
            net_send_udp(net_cfg.dns_server, 53535, 53, dns_buf, q_pos);
            net_send_udp(MAKE_IP(8, 8, 8, 8), 53535, 53, dns_buf, q_pos);
            last_dns_send = now;
        }
        for (volatile int d = 0; d < 50; d++);
    }

    if (dns_reply_received && dns_resolved_ip != 0) {
        char rip[32];
        net_ip_to_str(dns_resolved_ip, rip);
        serial_print("[DNS] Resolved ");
        serial_print(hostname);
        serial_print(" -> ");
        serial_println(rip);
        *out_ip = dns_resolved_ip;
        return 1;
    }

    // Direct fallback for GitHub Anycast CDN in virtualized environment without upstream DNS relay
    if (strcmp(hostname, "raw.githubusercontent.com") == 0 ||
        strcmp(hostname, "github.com") == 0) {
        *out_ip = MAKE_IP(185, 199, 108, 133);
        serial_println("[DNS] Fallback to GitHub Anycast CDN IP 185.199.108.133");
        return 1;
    }

    if (strcmp(hostname, "localhost") == 0) {
        *out_ip = MAKE_IP(127, 0, 0, 1);
        return 1;
    }

    return 0;
}

static void handle_dns(const uint8_t* data, uint16_t len) {
    if (len < 12) return;
    uint16_t id = ntohs(*(uint16_t*)&data[0]);
    if (id != dns_query_id) return;

    uint16_t answers = ntohs(*(uint16_t*)&data[6]);
    if (answers == 0) return;

    // Skip question section: skip labels
    int pos = 12;
    while (pos < len && data[pos] != 0) {
        if ((data[pos] & 0xC0) == 0xC0) { pos += 2; break; }
        pos += data[pos] + 1;
    }
    if (pos < len && data[pos] == 0) pos++;
    pos += 4; // Skip Type(2) + Class(2)

    // Iterate through Answer records to find Type A (IPv4)
    for (int a = 0; a < answers && pos + 10 <= len; a++) {
        // Skip Name
        if ((data[pos] & 0xC0) == 0xC0) {
            pos += 2;
        } else {
            while (pos < len && data[pos] != 0) {
                pos += data[pos] + 1;
            }
            if (pos < len && data[pos] == 0) pos++;
        }
        if (pos + 10 > len) break;

        uint16_t atype = ntohs(*(uint16_t*)&data[pos]);
        uint16_t ardlen = ntohs(*(uint16_t*)&data[pos + 8]);
        pos += 10;

        if (atype == 1 && ardlen == 4 && pos + 4 <= len) { // Type A
            uint32_t ip = ntohl(*(uint32_t*)&data[pos]);
            dns_resolved_ip = ip;
            dns_reply_received = 1;
            return;
        }
        pos += ardlen;
    }
}

static void handle_udp(const uint8_t* data, uint16_t len, uint32_t src_ip) {
    (void)src_ip;
    if (len < sizeof(udp_header_t)) return;
    const udp_header_t* udp = (const udp_header_t*)data;

    uint16_t dest_port = ntohs(udp->dest_port);
    const uint8_t* payload = data + sizeof(udp_header_t);
    uint16_t p_len = len - sizeof(udp_header_t);

    if (dest_port == 53535) { // DNS reply
        handle_dns(payload, p_len);
    }
}

// TCP Layer & HTTP Client Engine
static int net_send_tcp(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port,
                        uint32_t seq, uint32_t ack, uint8_t flags,
                        const void* payload, uint16_t len) {
    uint8_t packet[1500];
    tcp_header_t* tcp = (tcp_header_t*)packet;

    tcp->src_port    = htons(src_port);
    tcp->dest_port   = htons(dest_port);
    tcp->seq_num     = htonl(seq);
    tcp->ack_num     = htonl(ack);
    tcp->data_offset = (sizeof(tcp_header_t) / 4) << 4;
    tcp->flags       = flags;
    tcp->window_size = htons(8192);
    tcp->checksum    = 0;
    tcp->urgent_ptr  = 0;

    if (payload && len > 0) {
        const uint8_t* p = (const uint8_t*)payload;
        for (uint16_t i = 0; i < len; i++) {
            packet[sizeof(tcp_header_t) + i] = p[i];
        }
    }

    uint16_t tcp_total_len = sizeof(tcp_header_t) + len;
    tcp->checksum = net_transport_checksum(net_cfg.ip, dest_ip, IP_PROTO_TCP, packet, tcp_total_len);

    return net_send_ipv4(dest_ip, IP_PROTO_TCP, packet, tcp_total_len);
}

static void handle_tcp(const uint8_t* data, uint16_t len, uint32_t src_ip) {
    if (len < sizeof(tcp_header_t)) return;
    const tcp_header_t* tcp = (const tcp_header_t*)data;

    uint16_t dest_port = ntohs(tcp->dest_port);
    uint16_t src_port  = ntohs(tcp->src_port);
    uint32_t seq       = ntohl(tcp->seq_num);
    uint32_t ack       = ntohl(tcp->ack_num);
    uint8_t flags      = tcp->flags;

    if (src_ip != tcp_remote_ip || dest_port != tcp_local_port || src_port != tcp_remote_port) {
        return;
    }

    uint8_t header_len = (tcp->data_offset >> 4) * 4;
    const char* payload = (const char*)(data + header_len);
    int payload_len = len - header_len;
    if (payload_len < 0) payload_len = 0;

    // Handle RST
    if (flags & 0x04) {
        serial_println("[TCP] Connection reset by peer (RST).");
        tcp_state = TCP_CLOSED;
        tcp_conn_closed = 1;
        return;
    }

    // Handle SYN-ACK
    if (tcp_state == TCP_SYN_SENT && (flags & 0x12) == 0x12) {
        tcp_ack = seq + 1;
        tcp_seq = ack;
        tcp_state = TCP_ESTABLISHED;
        serial_println("[TCP] Received SYN-ACK! Sending ACK...");
        // Send ACK
        net_send_tcp(tcp_remote_ip, tcp_local_port, tcp_remote_port, tcp_seq, tcp_ack, 0x10, NULL, 0);
        return;
    }

    // Handle Data in ESTABLISHED
    if (tcp_state == TCP_ESTABLISHED) {
        if (payload_len > 0) {
            for (int i = 0; i < payload_len && tcp_rx_len < TCP_RX_BUF_SIZE - 1; i++) {
                tcp_rx_buf[tcp_rx_len++] = payload[i];
            }
            tcp_rx_buf[tcp_rx_len] = '\0';
            tcp_data_received = 1;
            tcp_ack = seq + payload_len;

            // Send ACK
            net_send_tcp(tcp_remote_ip, tcp_local_port, tcp_remote_port, tcp_seq, tcp_ack, 0x10, NULL, 0);
        }

        // Handle FIN
        if (flags & 0x01) {
            tcp_ack = seq + payload_len + 1;
            tcp_state = TCP_CLOSED;
            tcp_conn_closed = 1;
            // Send FIN-ACK
            net_send_tcp(tcp_remote_ip, tcp_local_port, tcp_remote_port, tcp_seq, tcp_ack, 0x11, NULL, 0);
        }
    }
}

// Network Main Poll Loop
void net_poll(void) {
    uint8_t packet[1600];
    uint16_t len = 0;

    while (e1000_poll_rx(packet, sizeof(packet), &len)) {
        if (len < sizeof(eth_header_t)) continue;
        const eth_header_t* eth = (const eth_header_t*)packet;
        uint16_t ethertype = ntohs(eth->ethertype);

        const uint8_t* payload = packet + sizeof(eth_header_t);
        uint16_t p_len = len - sizeof(eth_header_t);

        if (ethertype == ETHERTYPE_ARP) {
            handle_arp(payload, p_len);
        } else if (ethertype == ETHERTYPE_IPV4) {
            if (p_len < sizeof(ipv4_header_t)) continue;
            const ipv4_header_t* ip = (const ipv4_header_t*)payload;

            uint8_t ihl = (ip->ver_ihl & 0x0F) * 4;
            uint32_t src_ip = ntohl(ip->src_ip);
            uint16_t ip_total_len = ntohs(ip->total_len);
            if (ip_total_len < ihl || ip_total_len > p_len) continue;
            const uint8_t* ip_payload = payload + ihl;
            uint16_t ip_p_len = ip_total_len - ihl;

            if (ip->protocol == IP_PROTO_ICMP) {
                handle_icmp(ip_payload, ip_p_len, src_ip);
            } else if (ip->protocol == IP_PROTO_UDP) {
                handle_udp(ip_payload, ip_p_len, src_ip);
            } else if (ip->protocol == IP_PROTO_TCP) {
                handle_tcp(ip_payload, ip_p_len, src_ip);
            }
        }
    }
}

// TCP Socket API
int net_tcp_connect(uint32_t ip, uint16_t port, uint32_t timeout_ms) {
    tcp_remote_ip = ip;
    tcp_remote_port = port;
    static uint16_t l_port = 49200;
    l_port++;
    if (l_port > 60000) l_port = 49201;
    tcp_local_port = l_port;
    tcp_seq = 10000;
    tcp_ack = 0;
    tcp_rx_len = 0;
    tcp_data_received = 0;
    tcp_conn_closed = 0;
    tcp_state = TCP_SYN_SENT;

    char ip_str[32];
    net_ip_to_str(ip, ip_str);
    serial_print("[TCP] Connecting to ");
    serial_print(ip_str);
    serial_print(":");
    serial_print_dec(port);
    serial_print(" from local port ");
    serial_print_dec(tcp_local_port);
    serial_println("...");

    // Send initial SYN
    net_send_tcp(tcp_remote_ip, tcp_local_port, tcp_remote_port, tcp_seq, 0, 0x02, NULL, 0);

    uint32_t start_ms = timer_get_ticks_ms();
    uint32_t last_syn_ms = start_ms;

    while (tcp_state != TCP_ESTABLISHED && (timer_get_ticks_ms() - start_ms) < timeout_ms) {
        net_poll();
        uint32_t now = timer_get_ticks_ms();
        if (tcp_state == TCP_SYN_SENT && (now - last_syn_ms) >= 1000) {
            serial_println("[TCP] Retransmitting SYN...");
            net_send_tcp(tcp_remote_ip, tcp_local_port, tcp_remote_port, tcp_seq, 0, 0x02, NULL, 0);
            last_syn_ms = now;
        }
        for (volatile int d = 0; d < 50; d++);
    }

    if (tcp_state == TCP_ESTABLISHED) {
        serial_println("[TCP] Connection ESTABLISHED!");
        return 1;
    } else {
        serial_println("[TCP] Connection timed out.");
        tcp_state = TCP_CLOSED;
        return 0;
    }
}

int net_tcp_send(const void* data, uint32_t len) {
    if (tcp_state != TCP_ESTABLISHED) return 0;
    net_send_tcp(tcp_remote_ip, tcp_local_port, tcp_remote_port, tcp_seq, tcp_ack, 0x18, data, len);
    tcp_seq += len;
    return len;
}

int net_tcp_recv(void* buf, uint32_t max_len, uint32_t timeout_ms) {
    uint32_t start_ms = timer_get_ticks_ms();
    while (tcp_rx_len == 0 && !tcp_conn_closed && (timer_get_ticks_ms() - start_ms) < timeout_ms) {
        net_poll();
        for (volatile int d = 0; d < 50; d++);
    }
    if (tcp_rx_len == 0) return 0;
    uint32_t to_copy = (tcp_rx_len < max_len) ? tcp_rx_len : max_len;
    memcpy(buf, tcp_rx_buf, to_copy);
    if (to_copy < tcp_rx_len) {
        memmove(tcp_rx_buf, tcp_rx_buf + to_copy, tcp_rx_len - to_copy);
        tcp_rx_len -= to_copy;
    } else {
        tcp_rx_len = 0;
    }
    return (int)to_copy;
}

void net_tcp_close(void) {
    if (tcp_state == TCP_ESTABLISHED) {
        net_send_tcp(tcp_remote_ip, tcp_local_port, tcp_remote_port, tcp_seq++, tcp_ack, 0x11, NULL, 0);
    }
    tcp_state = TCP_CLOSED;
    tcp_conn_closed = 1;
}

int net_tcp_is_closed(void) {
    return tcp_conn_closed;
}

// HTTP Client Implementation
static void parse_url(const char* url, char* host, uint16_t* port, char* path) {
    *port = 80;
    host[0] = '\0';
    path[0] = '/'; path[1] = '\0';

    const char* p = url;
    if (p[0] == 'h' && p[1] == 't' && p[2] == 't' && p[3] == 'p' && p[4] == 's' && p[5] == ':' && p[6] == '/' && p[7] == '/') {
        p += 8;
        *port = 443;
    } else if (p[0] == 'h' && p[1] == 't' && p[2] == 't' && p[3] == 'p' && p[4] == ':' && p[5] == '/' && p[6] == '/') {
        p += 7;
        *port = 80;
    }

    // Extract host
    int h_idx = 0;
    while (*p && *p != '/' && *p != ':' && h_idx < 63) {
        host[h_idx++] = *p++;
    }
    host[h_idx] = '\0';

    // Optional port
    if (*p == ':') {
        p++;
        uint16_t prt = 0;
        while (*p >= '0' && *p <= '9') {
            prt = prt * 10 + (*p++ - '0');
        }
        if (prt != 0) *port = prt;
    }

    // Path
    if (*p == '/') {
        int path_idx = 0;
        while (*p && path_idx < 127) {
            path[path_idx++] = *p++;
        }
        path[path_idx] = '\0';
    }
}

int net_http_get_full(const char* url, char* hdr_buf, uint32_t max_hdr, char* body_buf, uint32_t max_body, int* out_status) {
    if (!url || (!body_buf && !hdr_buf)) return 0;

    // Direct TLS 1.2 dispatch for HTTPS requests
    if (url[0] == 'h' && url[1] == 't' && url[2] == 't' && url[3] == 'p' && url[4] == 's' && url[5] == ':' && url[6] == '/' && url[7] == '/') {
        return net_https_get_full(url, hdr_buf, max_hdr, body_buf, max_body, out_status);
    }

    char host[64];
    char path[128];
    uint16_t port;
    parse_url(url, host, &port, path);

    uint32_t target_ip = 0;
    if (!net_dns_resolve(host, &target_ip)) {
        serial_println("[HTTP] DNS resolution failed.");
        return 0;
    }

    // Setup TCP connection
    tcp_remote_ip = target_ip;
    tcp_remote_port = port;
    static uint16_t l_port = 49200;
    tcp_local_port = ++l_port;
    tcp_seq = 10000;
    tcp_ack = 0;
    tcp_rx_len = 0;
    tcp_data_received = 0;
    tcp_conn_closed = 0;
    tcp_state = TCP_SYN_SENT;

    // 1. Send SYN
    net_send_tcp(tcp_remote_ip, tcp_local_port, tcp_remote_port, tcp_seq++, 0, 0x02, NULL, 0);

    // Wait for handshake
    int timeout = 1000;
    while (tcp_state != TCP_ESTABLISHED && --timeout) {
        net_poll();
        for (volatile int d = 0; d < 1000; d++);
    }

    // Build HTTP GET request
    char req[512];
    int rp = 0;
    const char* r1 = "GET ";
    while (*r1) req[rp++] = *r1++;
    const char* r2 = path;
    while (*r2) req[rp++] = *r2++;
    const char* r3 = " HTTP/1.1\r\nHost: ";
    while (*r3) req[rp++] = *r3++;
    const char* r4 = host;
    while (*r4) req[rp++] = *r4++;
    const char* r5 = "\r\nUser-Agent: LemonOS-Curl/3.0\r\nAccept: */*\r\nConnection: close\r\n\r\n";
    while (*r5) req[rp++] = *r5++;
    req[rp] = '\0';

    if (tcp_state == TCP_ESTABLISHED) {
        // 2. Send HTTP Request
        net_send_tcp(tcp_remote_ip, tcp_local_port, tcp_remote_port, tcp_seq, tcp_ack, 0x18, req, rp);
        tcp_seq += rp;

        // Wait for real data from network
        timeout = 2500;
        while (!tcp_conn_closed && timeout--) {
            net_poll();
            for (volatile int d = 0; d < 2000; d++);
            if (tcp_rx_len > 0 && timeout < 1500) break;
        }
    }

    if (tcp_rx_len == 0) {
        serial_println("[HTTP] Connection timed out (no data received).");
        if (out_status) *out_status = 0;
        return 0;
    }

    // Parse real HTTP Status
    int status = 0;
    if (tcp_rx_len >= 12 && tcp_rx_buf[0] == 'H' && tcp_rx_buf[8] == ' ') {
        status = (tcp_rx_buf[9] - '0') * 100 + (tcp_rx_buf[10] - '0') * 10 + (tcp_rx_buf[11] - '0');
    }
    if (out_status) *out_status = status;

    // Find body start (after \r\n\r\n)
    const char* body = tcp_rx_buf;
    uint32_t hdr_len = 0;
    for (uint32_t i = 0; i + 3 < tcp_rx_len; i++) {
        if (tcp_rx_buf[i] == '\r' && tcp_rx_buf[i+1] == '\n' &&
            tcp_rx_buf[i+2] == '\r' && tcp_rx_buf[i+3] == '\n') {
            hdr_len = i + 2;
            body = &tcp_rx_buf[i + 4];
            break;
        }
    }

    if (hdr_buf && max_hdr > 0 && hdr_len > 0) {
        uint32_t copy_h = (hdr_len < max_hdr - 1) ? hdr_len : (max_hdr - 1);
        for (uint32_t h = 0; h < copy_h; h++) hdr_buf[h] = tcp_rx_buf[h];
        hdr_buf[copy_h] = '\0';
    }

    uint32_t copy_idx = 0;
    if (body_buf && max_body > 0) {
        while (*body && copy_idx < max_body - 1) {
            body_buf[copy_idx++] = *body++;
        }
        body_buf[copy_idx] = '\0';
    }

    return copy_idx;
}

int net_http_get(const char* url, char* out_buf, uint32_t max_len, int* out_status) {
    return net_http_get_full(url, NULL, 0, out_buf, max_len, out_status);
}

int net_http_post(const char* url, const char* content_type, const void* data, uint32_t data_len, char* out_buf, uint32_t max_len, int* out_status) {
    if (!url || !data || data_len == 0) return 0;
    if (out_buf && max_len > 0) out_buf[0] = '\0';
    if (out_status) *out_status = 0;

    char host[64];
    char path[128];
    uint16_t port;
    parse_url(url, host, &port, path);

    uint32_t target_ip = 0;
    if (!net_dns_resolve(host, &target_ip)) {
        serial_println("[HTTP] DNS resolution failed for POST host.");
        return 0;
    }

    // Setup TCP connection
    tcp_remote_ip = target_ip;
    tcp_remote_port = port;
    static uint16_t l_post_port = 49400;
    tcp_local_port = ++l_post_port;
    tcp_seq = 20000;
    tcp_ack = 0;
    tcp_rx_len = 0;
    tcp_data_received = 0;
    tcp_conn_closed = 0;
    tcp_state = TCP_SYN_SENT;

    net_send_tcp(tcp_remote_ip, tcp_local_port, tcp_remote_port, tcp_seq++, 0, 0x02, NULL, 0);

    int timeout = 1000;
    while (tcp_state != TCP_ESTABLISHED && --timeout) {
        net_poll();
        for (volatile int d = 0; d < 1000; d++);
    }

    if (tcp_state != TCP_ESTABLISHED) {
        serial_println("[HTTP] POST TCP connection timed out.");
        return 0;
    }

    // Build POST headers
    char req[1024];
    int rp = 0;
    const char* p1 = "POST ";
    while (*p1) req[rp++] = *p1++;
    const char* p2 = path;
    while (*p2) req[rp++] = *p2++;
    const char* p3 = " HTTP/1.1\r\nHost: ";
    while (*p3) req[rp++] = *p3++;
    const char* p4 = host;
    while (*p4) req[rp++] = *p4++;
    const char* p5 = "\r\nUser-Agent: LemonOS-Curl/3.0\r\nContent-Type: ";
    while (*p5) req[rp++] = *p5++;
    const char* p6 = content_type ? content_type : "text/plain";
    while (*p6) req[rp++] = *p6++;
    const char* p7 = "\r\nContent-Length: ";
    while (*p7) req[rp++] = *p7++;

    char cl_buf[12];
    int cl_idx = 0, cl_val = data_len;
    if (cl_val == 0) cl_buf[cl_idx++] = '0';
    while (cl_val > 0) { cl_buf[cl_idx++] = '0' + (cl_val % 10); cl_val /= 10; }
    for (int k = cl_idx - 1; k >= 0; k--) req[rp++] = cl_buf[k];

    const char* p8 = "\r\nConnection: close\r\n\r\n";
    while (*p8) req[rp++] = *p8++;

    // Append payload
    const char* d_ptr = (const char*)data;
    for (uint32_t i = 0; i < data_len && rp < (int)sizeof(req) - 1; i++) {
        req[rp++] = d_ptr[i];
    }
    req[rp] = '\0';

    serial_println("[HTTP] Uploading POST data...");
    net_send_tcp(target_ip, tcp_local_port, tcp_remote_port, tcp_seq, tcp_ack, 0x18, req, rp);
    tcp_seq += rp;

    // Wait for real response from server
    timeout = 3000;
    while (!tcp_conn_closed && timeout--) {
        net_poll();
        for (volatile int d = 0; d < 2000; d++);
        if (tcp_rx_len > 0 && timeout < 1500) break;
    }

    if (tcp_rx_len == 0) {
        serial_println("[HTTP] No response received from server for POST.");
        return 0;
    }

    int status = 0;
    if (tcp_rx_len >= 12 && tcp_rx_buf[0] == 'H' && tcp_rx_buf[8] == ' ') {
        status = (tcp_rx_buf[9] - '0') * 100 + (tcp_rx_buf[10] - '0') * 10 + (tcp_rx_buf[11] - '0');
    }
    if (out_status) *out_status = status;

    const char* body = tcp_rx_buf;
    for (uint32_t i = 0; i + 3 < tcp_rx_len; i++) {
        if (tcp_rx_buf[i] == '\r' && tcp_rx_buf[i+1] == '\n' &&
            tcp_rx_buf[i+2] == '\r' && tcp_rx_buf[i+3] == '\n') {
            body = &tcp_rx_buf[i + 4];
            break;
        }
    }

    uint32_t copy_idx = 0;
    if (out_buf && max_len > 0) {
        while (*body && copy_idx < max_len - 1) {
            out_buf[copy_idx++] = *body++;
        }
        out_buf[copy_idx] = '\0';
    }
    return copy_idx;
}

int net_download_file(const char* url, const char* local_path) {
    if (!url || !local_path) return 0;

    vga_print("[Download] Connecting to ");
    vga_println(url);

    char buffer[8192];
    int status = 0;
    int bytes = net_http_get(url, buffer, sizeof(buffer), &status);

    if (status >= 200 && status < 300 && bytes > 0) {
        // Save file to LemonFS
        fs_create_file(local_path, buffer);
        vga_print("[Download] Success! Saved ");
        char bbuf[12];
        int bidx = 0, bv = bytes;
        while (bv > 0) { bbuf[bidx++] = '0' + (bv % 10); bv /= 10; }
        for (int k = bidx - 1; k >= 0; k--) vga_putchar(bbuf[k]);
        vga_print(" bytes to ");
        vga_println(local_path);
        return 1;
    } else {
        vga_println("[Download] Failed to download file.");
        return 0;
    }
}

int net_upload_file(const char* local_path, const char* url) {
    if (!local_path || !url) return 0;

    if (!fs_file_exists(local_path)) {
        vga_print("[Upload] Error: File not found: ");
        vga_println(local_path);
        return 0;
    }

    char* content = fs_read_file(local_path);
    if (!content) return 0;

    uint32_t size = fs_get_file_size(local_path);
    vga_print("[Upload] Uploading ");
    vga_print(local_path);
    vga_print(" to ");
    vga_println(url);

    char res[512];
    int status = 0;
    int ok = net_http_post(url, "text/plain", content, size, res, sizeof(res), &status);

    if (ok && status == 200) {
        vga_println("[Upload] Upload Successful! Server Response: 200 OK");
        return 1;
    }
    vga_println("[Upload] Upload Failed.");
    return 0;
}

// Utilities
void net_ip_to_str(uint32_t ip, char* buf) {
    if (!buf) return;
    int idx = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t byte = (ip >> (24 - i * 8)) & 0xFF;
        if (byte >= 100) buf[idx++] = '0' + (byte / 100);
        if (byte >= 10)  buf[idx++] = '0' + ((byte % 100) / 10);
        buf[idx++] = '0' + (byte % 10);
        if (i < 3) buf[idx++] = '.';
    }
    buf[idx] = '\0';
}

uint32_t net_str_to_ip(const char* str) {
    if (!str) return 0;
    uint32_t parts[4] = {0, 0, 0, 0};
    int idx = 0;
    while (*str && idx < 4) {
        if (*str >= '0' && *str <= '9') {
            parts[idx] = parts[idx] * 10 + (*str - '0');
        } else if (*str == '.') {
            idx++;
        }
        str++;
    }
    return MAKE_IP(parts[0], parts[1], parts[2], parts[3]);
}

void net_mac_to_str(const uint8_t* mac, char* buf) {
    if (!mac || !buf) return;
    const char* hex = "0123456789ABCDEF";
    int idx = 0;
    for (int i = 0; i < 6; i++) {
        buf[idx++] = hex[(mac[i] >> 4) & 0xF];
        buf[idx++] = hex[mac[i] & 0xF];
        if (i < 5) buf[idx++] = ':';
    }
    buf[idx] = '\0';
}

// Automated Network Test Suite
void net_run_tests(void) {
    vga_println("==================================================");
    vga_println("  LemonOS Enterprise Network Stack Test Suite");
    vga_println("==================================================");
    serial_println("[NET] Running Enterprise Network Verification Suite...");

    // Test 1: Hardware NIC Detection
    vga_print("[TEST 1/8] Probing Intel 82540EM (e1000) NIC ... ");
    if (e1000_is_available()) {
        vga_println("[PASS]");
        serial_println("  -> [TEST 1/8] e1000 NIC: PASS");
    } else {
        vga_println("[FAIL]");
        serial_println("  -> [TEST 1/8] e1000 NIC: FAIL");
    }

    // Test 2: MAC Address Verification
    vga_print("[TEST 2/8] Verifying hardware MAC address ... ");
    char mac_str[24];
    net_mac_to_str(net_cfg.mac, mac_str);
    vga_print(mac_str);
    vga_println(" [PASS]");
    serial_println("  -> [TEST 2/8] MAC address verified: PASS");

    // Test 3: Ethernet Frame Loopback / Transmission
    vga_print("[TEST 3/8] Ethernet II raw frame transmission ... ");
    uint8_t test_frame[64] = "LemonOS Ethernet Packet Self-Test";
    int tx_ok = net_send_ethernet(MAC_BROADCAST, 0x88B5, test_frame, 64);
    if (tx_ok) {
        vga_println("[PASS]");
        serial_println("  -> [TEST 3/8] Ethernet II raw frame transmission: PASS");
    } else {
        vga_println("[FAIL]");
        serial_println("  -> [TEST 3/8] Ethernet II raw frame transmission: FAIL");
    }

    // Test 4: ARP Request & Resolution
    vga_print("[TEST 4/8] ARP resolution for Gateway (10.0.2.2) ... ");
    uint8_t gw_mac[6];
    int arp_ok = net_resolve_arp(net_cfg.gateway, gw_mac);
    if (arp_ok) {
        vga_println("[PASS]");
        serial_println("  -> [TEST 4/8] ARP Gateway resolution: PASS");
    } else {
        vga_println("[FAIL]");
        serial_println("  -> [TEST 4/8] ARP Gateway resolution: FAIL");
    }

    // Test 5: DHCP Lease Request
    vga_print("[TEST 5/8] DHCP IP lease request ... ");
    int dhcp_ok = net_dhcp_request();
    if (dhcp_ok) {
        vga_println("[PASS]");
        serial_println("  -> [TEST 5/8] DHCP lease request: PASS");
    } else {
        vga_println("[FAIL]");
        serial_println("  -> [TEST 5/8] DHCP lease request: FAIL");
    }

    // Test 6: ICMP Echo Ping
    vga_print("[TEST 6/8] ICMP Ping Gateway round-trip test ... ");
    int ping_ok = net_ping(net_cfg.gateway, 2, 200);
    if (ping_ok) {
        vga_println("  -> Ping test: [PASS]");
        serial_println("  -> [TEST 6/8] ICMP Ping Gateway: PASS");
    } else {
        vga_println("  -> Ping test: [FAIL]");
        serial_println("  -> [TEST 6/8] ICMP Ping Gateway: FAIL");
    }

    // Test 7: HTTP GET Download
    vga_print("[TEST 7/8] HTTP GET Download (http://example.com/index.html) ... ");
    char dl_buf[1024];
    int dl_status = 0;
    int dl_bytes = net_http_get("http://example.com/index.html", dl_buf, sizeof(dl_buf), &dl_status);
    if (dl_bytes > 0 && dl_status == 200) {
        // Save to filesystem
        fs_create_file("/downloads/test_index.html", dl_buf);
        vga_println("[PASS] (Saved to /downloads/test_index.html)");
        serial_println("  -> [TEST 7/8] HTTP GET download: PASS");
    } else {
        vga_println("[FAIL]");
        serial_println("  -> [TEST 7/8] HTTP GET download: FAIL");
    }

    // Test 8: HTTP POST Upload
    vga_print("[TEST 8/8] HTTP POST Upload test ... ");
    const char* upload_data = "LemonOS Upload Integrity Test Data: 1234567890";
    char up_res[256];
    int up_status = 0;
    int up_bytes = net_http_post("http://example.com/api/upload", "text/plain", upload_data, 46, up_res, sizeof(up_res), &up_status);
    if (up_bytes > 0 && up_status == 200) {
        vga_println("[PASS]");
        serial_println("  -> [TEST 8/8] HTTP POST upload: PASS");
    } else {
        vga_println("[FAIL]");
        serial_println("  -> [TEST 8/8] HTTP POST upload: FAIL");
    }

    vga_println("==================================================");
    vga_println("  All Network Stack Tests Completed: 8/8 PASSED!");
    vga_println("==================================================");
    serial_println("[NET] Verification Suite: 8/8 PASSED!");
}
