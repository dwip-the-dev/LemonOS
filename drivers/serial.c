#include "serial.h"
#include <stdint.h>

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);    // Disable all interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(COM1 + 1, 0x00);    //                  (hi byte)
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

void serial_putchar(char c) {
    while ((inb(COM1 + 5) & 0x20) == 0);
    outb(COM1, c);
}

void serial_print(const char* str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*str++);
    }
}

void serial_println(const char* str) {
    serial_print(str);
    serial_print("\n");
}

void serial_print_hex(uint32_t val) {
    serial_print("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nibble = (val >> i) & 0xF;
        if (nibble < 10) {
            serial_putchar('0' + nibble);
        } else {
            serial_putchar('A' + (nibble - 10));
        }
    }
}

void serial_print_dec(int val) {
    if (val < 0) {
        serial_putchar('-');
        val = -val;
    }
    char buf[12];
    int idx = 0;
    if (val == 0) {
        serial_putchar('0');
        return;
    }
    while (val > 0) {
        buf[idx++] = '0' + (val % 10);
        val /= 10;
    }
    for (int i = idx - 1; i >= 0; i--) {
        serial_putchar(buf[i]);
    }
}

int serial_has_char(void) {
    return (inb(COM1 + 5) & 0x01) != 0;
}

char serial_getchar(void) {
    while (!serial_has_char());
    return (char)inb(COM1);
}
