#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,

    // alias
    VGA_COLOR_YELLOW = 14
};

// Function declarations...
void vga_initialize(void);
void vga_set_cursor(int x, int y);
void vga_putchar_at(int x, int y, char c);
void vga_scroll(void);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_set_foreground(uint8_t color);
void vga_set_background(uint8_t color);
void vga_reset_color(void);
void vga_putchar_color(char c, uint8_t fg, uint8_t bg);
void vga_print_color(const char* s, uint8_t fg, uint8_t bg);
void vga_println_color(const char* s, uint8_t fg, uint8_t bg);
uint8_t vga_get_foreground(void);
uint8_t vga_get_background(void);
void vga_putchar(char c);
void vga_print(const char* s);
void vga_println(const char* s);
void vga_clear(void);

#endif // VGA_H
