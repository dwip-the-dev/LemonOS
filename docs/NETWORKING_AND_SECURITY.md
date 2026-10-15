# LemonOS Network Stack & Cryptographic TLS Engine

**Author:** Dwip Biswas ([@dwip-the-dev](https://github.com/dwip-the-dev))  
**Target:** Intel 82540EM (e1000) & Baremetal Network Layer  
**Standards:** RFC 791 (IPv4), RFC 792 (ICMP), RFC 793 (TCP), RFC 768 (UDP), RFC 1035 (DNS), RFC 5246 (TLS 1.2), RFC 7748 (Curve25519), NIST SP 800-38D (AES-GCM)  

---

## 1. Network Interface Card (Intel 82540EM)

- **File:** `drivers/e1000.c`, `include/e1000.h`
- **PCI Discovery:** Scans PCI configuration space for Vendor ID `0x8086` and Device ID `0x100E`.
- **Memory-Mapped I/O (MMIO):** Maps PCI BAR0 (128 KB aperture) into high kernel virtual memory.
- **DMA Rings:**
  - **Receive (RX) Ring:** Circular buffer of 32 descriptors (`e1000_rx_desc`) backed by 2048-byte physical frame buffers.
  - **Transmit (TX) Ring:** Circular buffer of 16 descriptors (`e1000_tx_desc`) with automatic end-of-packet (`EOP`) signaling.
- **Interrupts:** Hardware IRQ 11 triggers `e1000_handler()` to process received frames.

---

## 2. Protocol Stack (`drivers/net.c`, `include/net.h`)

The networking stack implements a complete zero-copy packet pipeline:

```
+-----------------------------------------------------------+
|    Application Layer (cURL, HTTP, HTTPS, NetSurf/Firefox) |
+-----------------------------------------------------------+
                              |
+-----------------------------------------------------------+
|    Transport Layer: TCP State Machine & UDP               |
|    - TCP 3-way Handshake (SYN, SYN-ACK, ACK, FIN)         |
|    - DNS Client Resolver (UDP port 53)                    |
|    - DHCP Lease Client (UDP port 67/68)                   |
+-----------------------------------------------------------+
                              |
+-----------------------------------------------------------+
|    Internet Layer: IPv4 & ICMP                            |
|    - IPv4 Packet Formatting & Internet Checksum (RFC 1071)|
|    - ICMP Echo Request / Reply (Ping)                     |
+-----------------------------------------------------------+
                              |
+-----------------------------------------------------------+
|    Link Layer: Ethernet II & ARP                          |
|    - ARP Table Resolution & Dynamic Cache (16 entries)    |
|    - Hardware MAC Address Filtering                       |
+-----------------------------------------------------------+
                              |
+-----------------------------------------------------------+
|    Hardware: Intel 82540EM e1000 DMA Controller           |
+-----------------------------------------------------------+
```

---

## 3. Baremetal TLS 1.2 Client Engine (`drivers/tls.c`, `include/tls.h`)

LemonOS includes a native baremetal implementation of Transport Layer Security (TLS 1.2) supporting modern cipher suite `TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256` (0xC02F):

### Cryptographic Components
1. **Elliptic Curve Diffie-Hellman (X25519 / Curve25519):**
   - High-speed Montgomery curve scalar multiplication (`x25519_scalarmult()`) over prime field $2^{255} - 19$.
   - Ephemeral key exchange generates forward-secret pre-master secrets with hardware/RDTSC entropy.
2. **Advanced Encryption Standard (AES-128):**
   - 10-round Rijndael cipher with 128-bit key expansion.
3. **Galois/Counter Mode (GCM) & GHASH:**
   - Authenticated Encryption with Associated Data (AEAD) implementing GF($2^{128}$) polynomial multiplication for cryptographic integrity and tamper detection.
4. **SHA-256 & PRF (Pseudorandom Function):**
   - RFC 5246 TLS PRF based on HMAC-SHA256 expands master secret into client/server write keys and 4-byte implicit IVs.

### Handshake Flow
1. **ClientHello:** Sends protocol version 0x0303 (TLS 1.2), 32-byte client random, SNI (Server Name Indication), Supported Groups (X25519), and Signature Algorithms extension.
2. **ServerHello & ServerKeyExchange:** Parses server random, extracts server ECDH public key (Curve25519) and RSA/ECDSA signature.
3. **ClientKeyExchange & Finished:** Computes shared secret via `x25519_scalarmult`, derives master secret, generates encrypted `Finished` verify data.
4. **Application Data:** All payload data sent/received is encrypted and authenticated with AES-128-GCM and explicit 8-byte nonces.
