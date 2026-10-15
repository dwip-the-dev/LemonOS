#include "bga.h"
#include <stdint.h>
#include <stddef.h>

static uint32_t current_width = 0;
static uint32_t current_height = 0;
static uint32_t current_bpp = 0;
static uint32_t* framebuffer_address = NULL;

// Port I/O helpers
static inline void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// BGA register access
void bga_write_register(uint16_t index, uint16_t value) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, value);
}

uint16_t bga_read_register(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

int bga_is_available(void) {
    uint16_t id = bga_read_register(VBE_DISPI_INDEX_ID);
    return (id >= 0xB0C0 && id <= 0xB0C6);
}

// PCI config read to locate the Linear Framebuffer (BAR0)
static uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1U << 31) | 
                                 ((uint32_t)bus << 16) | 
                                 ((uint32_t)slot << 11) | 
                                 ((uint32_t)func << 8) | 
                                 (offset & 0xFC));
    outl(0xCF8, address);
    return inl(0xCFC);
}

static uint32_t find_pci_framebuffer(void) {
    // Scan PCI bus for Bochs/QEMU VGA adapter: Vendor 0x1234, Device 0x1111
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t slot = 0; slot < 32; slot++) {
            uint32_t id = pci_read_config((uint8_t)bus, (uint8_t)slot, 0, 0x00);
            uint16_t vendor = id & 0xFFFF;
            uint16_t device = (id >> 16) & 0xFFFF;

            if (vendor == 0x1234 && (device == 0x1111 || device == 0x1234)) {
                // Found device, read BAR0
                uint32_t bar0 = pci_read_config((uint8_t)bus, (uint8_t)slot, 0, 0x10);
                uint32_t lfb_base = bar0 & 0xFFFFFFF0;
                if (lfb_base != 0) {
                    return lfb_base;
                }
            }
        }
    }
    // Fallback standard addresses used by QEMU and Bochs
    return 0xE0000000;
}

int bga_set_video_mode(uint16_t width, uint16_t height, uint16_t bpp, int use_lfb, int clear_mem) {
    if (!bga_is_available()) {
        return 0;
    }

    // Disable VBE first
    bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);

    // Set resolution and color depth
    bga_write_register(VBE_DISPI_INDEX_XRES, width);
    bga_write_register(VBE_DISPI_INDEX_YRES, height);
    bga_write_register(VBE_DISPI_INDEX_BPP, bpp);

    // Re-enable with linear framebuffer flag
    uint16_t flags = VBE_DISPI_ENABLED;
    if (use_lfb) {
        flags |= VBE_DISPI_LFB_ENABLED;
    }
    if (!clear_mem) {
        flags |= VBE_DISPI_NOCLEARMEM;
    }
    bga_write_register(VBE_DISPI_INDEX_ENABLE, flags);

    current_width = width;
    current_height = height;
    current_bpp = bpp;

    // Detect and cache linear framebuffer address
    uint32_t lfb_addr = find_pci_framebuffer();
    framebuffer_address = (uint32_t*)lfb_addr;

    return 1;
}

void bga_disable(void) {
    bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    current_width = 0;
    current_height = 0;
    current_bpp = 0;
    framebuffer_address = NULL;
}

uint32_t* bga_get_framebuffer(void) {
    return framebuffer_address;
}

uint32_t bga_get_width(void) {
    return current_width;
}

uint32_t bga_get_height(void) {
    return current_height;
}

uint32_t bga_get_bpp(void) {
    return current_bpp;
}
