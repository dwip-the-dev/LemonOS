#include "color_demo.h"
#include "vga.h"
#include "keyboard.h"

void color_demo(void) {
    vga_clear();
    
    vga_println_color("=== LEMONOS COLOR DEMO ===", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_println("");
    
    // Display all foreground colors
    vga_println_color("FOREGROUND COLORS:", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println("");
    
    vga_println_color("BLACK", VGA_COLOR_BLACK, VGA_COLOR_WHITE);
    vga_println_color("BLUE", VGA_COLOR_BLUE, VGA_COLOR_BLACK);
    vga_println_color("GREEN", VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    vga_println_color("CYAN", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    vga_println_color("RED", VGA_COLOR_RED, VGA_COLOR_BLACK);
    vga_println_color("MAGENTA", VGA_COLOR_MAGENTA, VGA_COLOR_BLACK);
    vga_println_color("BROWN", VGA_COLOR_BROWN, VGA_COLOR_BLACK);
    vga_println_color("LIGHT GREY", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_println_color("DARK GREY", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_println_color("LIGHT BLUE", VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_println_color("LIGHT GREEN", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_println_color("LIGHT CYAN", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_println_color("LIGHT RED", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_println_color("LIGHT MAGENTA", VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    vga_println_color("YELLOW", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_println_color("WHITE", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    vga_println("");
    vga_println_color("BACKGROUND COLORS:", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println("");
    
    // Display background colors
    vga_println_color("BLACK BACKGROUND", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("BLUE BACKGROUND", VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    vga_println_color("GREEN BACKGROUND", VGA_COLOR_BLACK, VGA_COLOR_GREEN);
    vga_println_color("CYAN BACKGROUND", VGA_COLOR_BLACK, VGA_COLOR_CYAN);
    vga_println_color("RED BACKGROUND", VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_println_color("MAGENTA BACKGROUND", VGA_COLOR_WHITE, VGA_COLOR_MAGENTA);
    
    vga_println("");
    vga_println_color("COLOR COMBINATIONS:", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println("");
    
    // Some nice combinations
    vga_println_color("Yellow on Blue", VGA_COLOR_YELLOW, VGA_COLOR_BLUE);
    vga_println_color("White on Red", VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_println_color("Black on Cyan", VGA_COLOR_BLACK, VGA_COLOR_CYAN);
    vga_println_color("Red on Green", VGA_COLOR_RED, VGA_COLOR_GREEN);
    vga_println_color("Cyan on Magenta", VGA_COLOR_CYAN, VGA_COLOR_MAGENTA);
    
    vga_println("");
    vga_println_color("Press any key to continue...", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    while (!has_key()) {
        poll_keyboard();
    }
    getchar();
    
    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK); // Reset to default
    vga_println("Color demo completed!");
}
