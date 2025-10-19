#ifndef VGA_H
#define VGA_H

void vga_initialize(void);  // Add this line
void vga_clear(void);
void vga_putchar(char c);
void vga_putchar_at(int x, int y, char c);
void vga_set_cursor(int x, int y);
void vga_print(const char* s);
void vga_println(const char* s);
void vga_scroll(void);

#endif
