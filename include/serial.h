#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_putchar(char c);
void serial_print(const char* str);
void serial_println(const char* str);
void serial_print_hex(uint32_t val);
void serial_print_dec(int val);
int  serial_has_char(void);
char serial_getchar(void);

#endif
