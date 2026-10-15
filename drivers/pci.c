#include "pci.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

static inline void outl(uint16_t port, uint32_t val) {
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1U << 31) |
                                 ((uint32_t)bus << 16) |
                                 ((uint32_t)slot << 11) |
                                 ((uint32_t)func << 8) |
                                 (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((1U << 31) |
                                 ((uint32_t)bus << 16) |
                                 ((uint32_t)slot << 11) |
                                 ((uint32_t)func << 8) |
                                 (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

uint16_t pci_read_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read_config(bus, slot, func, offset);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

void pci_write_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t current = pci_read_config(bus, slot, func, offset);
    if (offset & 2) {
        current = (current & 0x0000FFFF) | ((uint32_t)value << 16);
    } else {
        current = (current & 0xFFFF0000) | (uint32_t)value;
    }
    pci_write_config(bus, slot, func, offset, current);
}

void pci_init(void) {
    serial_println("[PCI] Scanning PCI bus...");
}

int pci_find_device(uint16_t vendor_id, uint16_t device_id, pci_device_t* dev) {
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t slot = 0; slot < 32; slot++) {
            for (uint32_t func = 0; func < 8; func++) {
                uint32_t id = pci_read_config((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x00);
                uint16_t vendor = id & 0xFFFF;
                uint16_t device = (id >> 16) & 0xFFFF;

                if (vendor == 0xFFFF) {
                    if (func == 0) break; // Device doesn't exist
                    continue;
                }

                if (vendor == vendor_id && (device_id == 0xFFFF || device == device_id)) {
                    if (dev) {
                        dev->bus = (uint8_t)bus;
                        dev->slot = (uint8_t)slot;
                        dev->func = (uint8_t)func;
                        dev->vendor_id = vendor;
                        dev->device_id = device;

                        uint32_t class_reg = pci_read_config((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x08);
                        dev->class_id = (class_reg >> 24) & 0xFF;
                        dev->subclass = (class_reg >> 16) & 0xFF;

                        dev->bar0 = pci_read_config((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x10);
                        dev->bar1 = pci_read_config((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x14);

                        uint32_t intr = pci_read_config((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x3C);
                        dev->irq = intr & 0xFF;
                    }
                    return 1; // Found
                }
            }
        }
    }
    return 0; // Not found
}

int pci_find_class(uint8_t class_id, uint8_t subclass, pci_device_t* dev) {
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t slot = 0; slot < 32; slot++) {
            for (uint32_t func = 0; func < 8; func++) {
                uint32_t id = pci_read_config((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x00);
                uint16_t vendor = id & 0xFFFF;
                if (vendor == 0xFFFF) {
                    if (func == 0) break;
                    continue;
                }

                uint32_t class_reg = pci_read_config((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x08);
                uint8_t c = (class_reg >> 24) & 0xFF;
                uint8_t sc = (class_reg >> 16) & 0xFF;

                if (c == class_id && (subclass == 0xFF || sc == subclass)) {
                    if (dev) {
                        dev->bus = (uint8_t)bus;
                        dev->slot = (uint8_t)slot;
                        dev->func = (uint8_t)func;
                        dev->vendor_id = vendor;
                        dev->device_id = (id >> 16) & 0xFFFF;
                        dev->class_id = c;
                        dev->subclass = sc;
                        dev->bar0 = pci_read_config((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x10);
                        dev->bar1 = pci_read_config((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x14);
                        dev->irq = pci_read_config((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x3C) & 0xFF;
                    }
                    return 1;
                }
            }
        }
    }
    return 0;
}

void pci_enable_bus_mastering(const pci_device_t* dev) {
    if (!dev) return;
    uint32_t cmd = pci_read_config(dev->bus, dev->slot, dev->func, 0x04);
    // Bit 0: I/O Space, Bit 1: Memory Space, Bit 2: Bus Master
    cmd |= 0x0007;
    pci_write_config(dev->bus, dev->slot, dev->func, 0x04, cmd);
}
