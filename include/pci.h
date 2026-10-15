#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

// PCI device descriptor
typedef struct {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_id;
    uint8_t  subclass;
    uint32_t bar0;
    uint32_t bar1;
    uint8_t  irq;
} pci_device_t;

// PCI configuration space access
uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);

uint16_t pci_read_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_write_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);

// PCI scanning and device lookup
void pci_init(void);
int  pci_find_device(uint16_t vendor_id, uint16_t device_id, pci_device_t* dev);
int  pci_find_class(uint8_t class_id, uint8_t subclass, pci_device_t* dev);
void pci_enable_bus_mastering(const pci_device_t* dev);

#endif
