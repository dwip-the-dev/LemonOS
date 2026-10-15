#include "e1000.h"
#include "pci.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

static int e1000_initialized = 0;
static volatile uint32_t* e1000_mmio_base = NULL;
static uint8_t mac_address[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56}; // QEMU default fallback

static uint32_t rx_packet_count = 0;
static uint32_t tx_packet_count = 0;

// Descriptor rings aligned to 16 bytes
static e1000_rx_desc_t rx_descriptors[E1000_NUM_RX_DESC] __attribute__((aligned(16)));
static e1000_tx_desc_t tx_descriptors[E1000_NUM_TX_DESC] __attribute__((aligned(16)));

// Packet buffers
static uint8_t rx_buffers[E1000_NUM_RX_DESC][E1000_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffers[E1000_NUM_TX_DESC][E1000_BUFFER_SIZE] __attribute__((aligned(16)));

static uint16_t rx_current = 0;
static uint16_t tx_current = 0;

// Software fallback loopback queue
#define LOOPBACK_QUEUE_SIZE 4
static uint8_t loopback_queue[LOOPBACK_QUEUE_SIZE][E1000_BUFFER_SIZE];
static uint16_t loopback_lengths[LOOPBACK_QUEUE_SIZE];
static int loopback_head = 0;
static int loopback_tail = 0;
static int loopback_count = 0;

static inline void mmio_write32(uint32_t reg, uint32_t val) {
    if (e1000_mmio_base) {
        *(volatile uint32_t*)((uintptr_t)e1000_mmio_base + reg) = val;
    }
}

static inline uint32_t mmio_read32(uint32_t reg) {
    if (e1000_mmio_base) {
        return *(volatile uint32_t*)((uintptr_t)e1000_mmio_base + reg);
    }
    return 0;
}

static uint16_t e1000_read_eeprom(uint8_t addr) {
    uint32_t temp = 0;
    mmio_write32(E1000_REG_EERD, 1 | ((uint32_t)addr << 8));
    int timeout = 100000;
    while (!((temp = mmio_read32(E1000_REG_EERD)) & (1 << 4)) && --timeout) {
        asm volatile("pause");
    }
    return (uint16_t)((temp >> 16) & 0xFFFF);
}

int e1000_init(void) {
    pci_device_t dev;

    serial_println("[E1000] Probing PCI for Intel 82540EM...");

    // Search for e1000 device IDs: 0x100E, 0x1004, 0x100F, 0x1010, etc.
    int found = pci_find_device(E1000_VENDOR_ID, E1000_DEV_82540EM, &dev);
    if (!found) {
        found = pci_find_device(E1000_VENDOR_ID, 0x1004, &dev);
    }
    if (!found) {
        found = pci_find_device(E1000_VENDOR_ID, 0x100F, &dev);
    }
    if (!found) {
        // Search by PCI class 0x02 (Network Controller), subclass 0x00 (Ethernet)
        found = pci_find_class(0x02, 0x00, &dev);
    }

    if (!found) {
        serial_println("[E1000] Notice: Hardware e1000 not found on PCI. Enabling loopback virtual NIC mode.");
        e1000_initialized = 1;
        return 1;
    }

    serial_print("[E1000] Found e1000 at Bus ");
    serial_print_dec(dev.bus);
    serial_print(", Slot ");
    serial_print_dec(dev.slot);
    serial_print(", Device ID: ");
    serial_print_hex(dev.device_id);
    serial_println("");

    // Enable bus mastering
    pci_enable_bus_mastering(&dev);

    // Get MMIO base address from BAR0
    uint32_t mmio_addr = dev.bar0 & ~0xF;
    e1000_mmio_base = (volatile uint32_t*)mmio_addr;
    serial_print("[E1000] MMIO Base: ");
    serial_print_hex(mmio_addr);
    serial_println("");

    // Read MAC address from EEPROM
    uint16_t word0 = e1000_read_eeprom(0);
    uint16_t word1 = e1000_read_eeprom(1);
    uint16_t word2 = e1000_read_eeprom(2);

    if (word0 != 0 && word0 != 0xFFFF) {
        mac_address[0] = word0 & 0xFF;
        mac_address[1] = (word0 >> 8) & 0xFF;
        mac_address[2] = word1 & 0xFF;
        mac_address[3] = (word1 >> 8) & 0xFF;
        mac_address[4] = word2 & 0xFF;
        mac_address[5] = (word2 >> 8) & 0xFF;
    } else {
        // Fallback: Read from RAL/RAH registers
        uint32_t ral = mmio_read32(E1000_REG_RAL);
        uint32_t rah = mmio_read32(E1000_REG_RAH);
        if (ral != 0) {
            mac_address[0] = ral & 0xFF;
            mac_address[1] = (ral >> 8) & 0xFF;
            mac_address[2] = (ral >> 16) & 0xFF;
            mac_address[3] = (ral >> 24) & 0xFF;
            mac_address[4] = rah & 0xFF;
            mac_address[5] = (rah >> 8) & 0xFF;
        }
    }

    serial_print("[E1000] MAC Address: ");
    for (int i = 0; i < 6; i++) {
        serial_print_hex(mac_address[i]);
        if (i < 5) serial_print(":");
    }
    serial_println("");

    // Initialize Multicast Table Array to 0
    for (int i = 0; i < 128; i++) {
        mmio_write32(E1000_REG_MTA + (i * 4), 0);
    }

    // Set link up in device control
    uint32_t ctrl = mmio_read32(E1000_REG_CTRL);
    ctrl |= (1 << 5) | (1 << 6); // Auto-speed detection, set link up
    mmio_write32(E1000_REG_CTRL, ctrl);

    // Setup RX descriptor ring
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        rx_descriptors[i].address = (uint32_t)&rx_buffers[i][0];
        rx_descriptors[i].status = 0;
        rx_descriptors[i].errors = 0;
    }
    rx_current = 0;

    mmio_write32(E1000_REG_RDBAL, (uint32_t)&rx_descriptors[0]);
    mmio_write32(E1000_REG_RDBAH, 0);
    mmio_write32(E1000_REG_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    mmio_write32(E1000_REG_RDH, 0);
    mmio_write32(E1000_REG_RDT, E1000_NUM_RX_DESC - 1);

    // Enable Receiver (2048-byte buffer, broadcast accept, multicast accept)
    mmio_write32(E1000_REG_RCTL, E1000_RCTL_EN | E1000_RCTL_SBP | E1000_RCTL_UPE |
                                 E1000_RCTL_MPE | E1000_RCTL_BAM | E1000_RCTL_BSIZE_2048 |
                                 E1000_RCTL_SECRC);

    // Setup TX descriptor ring
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        tx_descriptors[i].address = (uint32_t)&tx_buffers[i][0];
        tx_descriptors[i].status = 1; // Mark as free initially
        tx_descriptors[i].cmd = 0;
    }
    tx_current = 0;

    mmio_write32(E1000_REG_TDBAL, (uint32_t)&tx_descriptors[0]);
    mmio_write32(E1000_REG_TDBAH, 0);
    mmio_write32(E1000_REG_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    mmio_write32(E1000_REG_TDH, 0);
    mmio_write32(E1000_REG_TDT, 0);

    // Enable Transmitter
    // Pad short packets, Collision threshold 15, Collision distance 64
    mmio_write32(E1000_REG_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | (15 << 4) | (64 << 12));

    e1000_initialized = 1;
    serial_println("[E1000] Initialization complete. Link active.");
    return 1;
}

int e1000_is_available(void) {
    return e1000_initialized;
}

void e1000_get_mac(uint8_t* out_mac) {
    if (!out_mac) return;
    for (int i = 0; i < 6; i++) {
        out_mac[i] = mac_address[i];
    }
}

int e1000_send(const void* data, uint16_t len) {
    if (!e1000_initialized || !data || len == 0 || len > E1000_BUFFER_SIZE) {
        return 0;
    }

    if (e1000_mmio_base) {
        // Hardware transmit via TX ring
        uint16_t idx = tx_current;
        volatile e1000_tx_desc_t* desc = &tx_descriptors[idx];

        // Copy packet data to TX buffer
        const uint8_t* src = (const uint8_t*)data;
        uint8_t* dst = tx_buffers[idx];
        for (uint16_t i = 0; i < len; i++) {
            dst[i] = src[i];
        }

        desc->length = len;
        // CMD: End of Packet (EOP, bit 0), Insert FCS (IFCS, bit 1), Report Status (RS, bit 3)
        desc->cmd = 0x01 | 0x02 | 0x08;
        desc->status = 0;

        tx_current = (tx_current + 1) % E1000_NUM_TX_DESC;
        mmio_write32(E1000_REG_TDT, tx_current);

        // Wait for transmit complete
        int timeout = 100000;
        while (!(desc->status & 0x01) && --timeout) {
            asm volatile("pause");
        }

        tx_packet_count++;
        return (timeout > 0);
    } else {
        // Software loopback mode: enqueue to loopback buffer
        if (loopback_count < LOOPBACK_QUEUE_SIZE) {
            const uint8_t* src = (const uint8_t*)data;
            for (uint16_t i = 0; i < len; i++) {
                loopback_queue[loopback_tail][i] = src[i];
            }
            loopback_lengths[loopback_tail] = len;
            loopback_tail = (loopback_tail + 1) % LOOPBACK_QUEUE_SIZE;
            loopback_count++;
        }
        tx_packet_count++;
        return 1;
    }
}

int e1000_poll_rx(void* buffer, uint16_t max_len, uint16_t* out_len) {
    if (!e1000_initialized || !buffer || !out_len) {
        return 0;
    }

    if (e1000_mmio_base) {
        volatile e1000_rx_desc_t* desc = &rx_descriptors[rx_current];
        // Check Descriptor Done (DD, bit 0)
        if (!(desc->status & 0x01)) {
            return 0; // No packet available
        }

        uint16_t len = desc->length;
        if (len > max_len) len = max_len;

        uint8_t* dst = (uint8_t*)buffer;
        uint8_t* src = rx_buffers[rx_current];
        for (uint16_t i = 0; i < len; i++) {
            dst[i] = src[i];
        }
        *out_len = len;

        // Reset descriptor
        desc->status = 0;
        uint16_t prev = rx_current;
        rx_current = (rx_current + 1) % E1000_NUM_RX_DESC;
        mmio_write32(E1000_REG_RDT, prev);

        rx_packet_count++;
        return 1;
    } else {
        // Check loopback queue
        if (loopback_count > 0) {
            uint16_t len = loopback_lengths[loopback_head];
            if (len > max_len) len = max_len;

            uint8_t* dst = (uint8_t*)buffer;
            for (uint16_t i = 0; i < len; i++) {
                dst[i] = loopback_queue[loopback_head][i];
            }
            *out_len = len;

            loopback_head = (loopback_head + 1) % LOOPBACK_QUEUE_SIZE;
            loopback_count--;
            rx_packet_count++;
            return 1;
        }
        return 0;
    }
}

int e1000_get_link_status(void) {
    if (!e1000_initialized) return 0;
    if (e1000_mmio_base) {
        uint32_t status = mmio_read32(E1000_REG_STATUS);
        return (status & (1 << 1)) != 0; // Bit 1: Link Up
    }
    return 1; // Loopback always linked
}

uint32_t e1000_get_rx_count(void) {
    return rx_packet_count;
}

uint32_t e1000_get_tx_count(void) {
    return tx_packet_count;
}
