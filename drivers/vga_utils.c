#include "vga_utils.h"
#include "vga.h"
#include <stdint.h>  // Add this line

// Simple backspace implementation for VGA
void vga_backspace(void) {
    vga_putchar('\b');
    vga_putchar(' ');
    vga_putchar('\b');
}

// Print integer to VGA
void vga_print_int(int num) {
    if (num == 0) {
        vga_putchar('0');
        return;
    }
    
    if (num < 0) {
        vga_putchar('-');
        num = -num;
    }
    
    char buffer[16];
    int i = 0;
    
    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    while (i > 0) {
        vga_putchar(buffer[--i]);
    }
}

// Print hexadecimal number
void vga_print_hex(uint32_t num) {
    char hex_chars[] = "0123456789ABCDEF";
    char buffer[9];
    int i;
    
    for (i = 7; i >= 0; i--) {
        buffer[i] = hex_chars[num & 0xF];
        num >>= 4;
    }
    buffer[8] = '\0';
    
    // Skip leading zeros
    int start = 0;
    while (start < 7 && buffer[start] == '0') {
        start++;
    }
    
    for (i = start; i < 8; i++) {
        vga_putchar(buffer[i]);
    }
}
