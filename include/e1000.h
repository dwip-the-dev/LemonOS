#ifndef E1000_H
#define E1000_H

#include <stdint.h>
#include <stddef.h>

#define E1000_VENDOR_ID 0x8086
#define E1000_DEV_82540EM 0x100E

// Register offsets
#define E1000_REG_CTRL      0x0000
#define E1000_REG_STATUS    0x0008
#define E1000_REG_EERD      0x0014
#define E1000_REG_ICR       0x00C0
#define E1000_REG_IMS       0x00D0
#define E1000_REG_RCTL      0x0100
#define E1000_REG_TCTL      0x0400
#define E1000_REG_RDBAL     0x2800
#define E1000_REG_RDBAH     0x2804
#define E1000_REG_RDLEN     0x2808
#define E1000_REG_RDH       0x2810
#define E1000_REG_RDT       0x2818
#define E1000_REG_TDBAL     0x3800
#define E1000_REG_TDBAH     0x3804
#define E1000_REG_TDLEN     0x3808
#define E1000_REG_TDH       0x3810
#define E1000_REG_TDT       0x3818
#define E1000_REG_MTA       0x5200
#define E1000_REG_RAL       0x5400
#define E1000_REG_RAH       0x5404

// RCTL flags
#define E1000_RCTL_EN       (1 << 1)
#define E1000_RCTL_SBP      (1 << 2)
#define E1000_RCTL_UPE      (1 << 3)
#define E1000_RCTL_MPE      (1 << 4)
#define E1000_RCTL_LPE      (1 << 5)
#define E1000_RCTL_BAM      (1 << 15)
#define E1000_RCTL_BSIZE_2048 (0 << 16)
#define E1000_RCTL_SECRC    (1 << 26)

// TCTL flags
#define E1000_TCTL_EN       (1 << 1)
#define E1000_TCTL_PSP      (1 << 3)

// Number of descriptors
#define E1000_NUM_RX_DESC   32
#define E1000_NUM_TX_DESC   32
#define E1000_BUFFER_SIZE   2048

// RX descriptor format
typedef struct __attribute__((packed)) {
    uint64_t address;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} e1000_rx_desc_t;

// TX descriptor format
typedef struct __attribute__((packed)) {
    uint64_t address;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} e1000_tx_desc_t;

// Function prototypes
int  e1000_init(void);
int  e1000_is_available(void);
void e1000_get_mac(uint8_t* out_mac);
int  e1000_send(const void* data, uint16_t len);
int  e1000_poll_rx(void* buffer, uint16_t max_len, uint16_t* out_len);
int  e1000_get_link_status(void);
uint32_t e1000_get_rx_count(void);
uint32_t e1000_get_tx_count(void);

#endif
