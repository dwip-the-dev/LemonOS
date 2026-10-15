#ifndef BGA_H
#define BGA_H

#include <stdint.h>
#include <stddef.h>

#define BGA_WIDTH  1024
#define BGA_HEIGHT 768
#define BGA_BPP    32

// BGA I/O ports
#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF

// BGA register indices
#define VBE_DISPI_INDEX_ID          0
#define VBE_DISPI_INDEX_XRES        1
#define VBE_DISPI_INDEX_YRES        2
#define VBE_DISPI_INDEX_BPP         3
#define VBE_DISPI_INDEX_ENABLE      4
#define VBE_DISPI_INDEX_BANK        5
#define VBE_DISPI_INDEX_VIRT_WIDTH  6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 7
#define VBE_DISPI_INDEX_X_OFFSET    8
#define VBE_DISPI_INDEX_Y_OFFSET    9

// BGA flags
#define VBE_DISPI_DISABLED    0x00
#define VBE_DISPI_ENABLED     0x01
#define VBE_DISPI_LFB_ENABLED 0x40
#define VBE_DISPI_NOCLEARMEM  0x80

// Function prototypes
int bga_is_available(void);
void bga_write_register(uint16_t index, uint16_t value);
uint16_t bga_read_register(uint16_t index);
int bga_set_video_mode(uint16_t width, uint16_t height, uint16_t bpp, int use_lfb, int clear_mem);
void bga_disable(void);
uint32_t* bga_get_framebuffer(void);
uint32_t bga_get_width(void);
uint32_t bga_get_height(void);
uint32_t bga_get_bpp(void);

#endif
