#ifndef VGA_UTILS_H
#define VGA_UTILS_H

#include <stdint.h>  // Add this line

void vga_backspace(void);
void vga_print_int(int num);
void vga_print_hex(uint32_t num);

#endif
