#include "gui.h"
#include "vga.h"
#include "keyboard.h"
#include "vga_utils.h"
#include <stdint.h>

// GUI state
static int gui_initialized = 0;
static gui_pixel_t gui_buffer[GUI_HEIGHT][GUI_WIDTH];

// Simple absolute value function
static int gui_abs(int x) {
    return (x < 0) ? -x : x;
}

void gui_init(void) {
    // Clear the GUI buffer
    for (int y = 0; y < GUI_HEIGHT; y++) {
        for (int x = 0; x < GUI_WIDTH; x++) {
            gui_buffer[y][x] = GUI_EMPTY;
        }
    }
    gui_initialized = 1;
}

void gui_exit(void) {
    gui_initialized = 0;
    vga_clear();
}

void gui_draw_pixel(int x, int y, gui_pixel_t pixel) {
    if (x >= 0 && x < GUI_WIDTH && y >= 0 && y < GUI_HEIGHT && gui_initialized) {
        gui_buffer[y][x] = pixel;
    }
}

void gui_draw_rect(int x, int y, int width, int height, gui_pixel_t pixel) {
    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            gui_draw_pixel(x + dx, y + dy, pixel);
        }
    }
}

void gui_draw_line(int x1, int y1, int x2, int y2, gui_pixel_t pixel) {
    int dx = gui_abs(x2 - x1);
    int dy = gui_abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        gui_draw_pixel(x1, y1, pixel);
        if (x1 == x2 && y1 == y2) break;
        int e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void gui_draw_circle(int cx, int cy, int radius, gui_pixel_t pixel) {
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y) {
        gui_draw_pixel(cx + x, cy + y, pixel);
        gui_draw_pixel(cx + y, cy + x, pixel);
        gui_draw_pixel(cx - y, cy + x, pixel);
        gui_draw_pixel(cx - x, cy + y, pixel);
        gui_draw_pixel(cx - x, cy - y, pixel);
        gui_draw_pixel(cx - y, cy - x, pixel);
        gui_draw_pixel(cx + y, cy - x, pixel);
        gui_draw_pixel(cx + x, cy - y, pixel);
        
        y++;
        err += 1 + 2*y;
        if (2*(err - x) + 1 > 0) {
            x--;
            err += 1 - 2*x;
        }
    }
}

void gui_draw_text(int x, int y, const char* text) {
    int pos = 0;
    int start_x = x;
    
    while (text[pos] != '\0') {
        if (text[pos] == '\n') {
            y++;
            x = start_x;
        } else {
            gui_draw_pixel(x, y, text[pos]);
            x++;
        }
        pos++;
    }
}

void gui_clear_screen(void) {
    for (int y = 0; y < GUI_HEIGHT; y++) {
        for (int x = 0; x < GUI_WIDTH; x++) {
            gui_buffer[y][x] = GUI_EMPTY;
        }
    }
}

void gui_render(void) {
    if (!gui_initialized) return;
    
    vga_clear();
    
    for (int y = 0; y < GUI_HEIGHT; y++) {
        for (int x = 0; x < GUI_WIDTH; x++) {
            vga_putchar_at(x, y, gui_buffer[y][x]);
        }
    }
    
    vga_set_cursor(0, 0);
}

// Simple GUI demo with windows and buttons
void gui_demo(void) {
    gui_init();
    
    // Draw a window
    gui_draw_rect(5, 3, 70, 18, GUI_FILL);
    gui_draw_rect(6, 4, 68, 16, GUI_EMPTY);
    
    // Window title
    gui_draw_text(10, 5, "LemonOS GUI Demo");
    gui_draw_line(6, 6, 73, 6, GUI_LINE);
    
    // Draw some buttons
    gui_draw_rect(10, 8, 15, 3, GUI_HALF);
    gui_draw_text(12, 9, "Button 1");
    
    gui_draw_rect(30, 8, 15, 3, GUI_HALF);
    gui_draw_text(32, 9, "Button 2");
    
    gui_draw_rect(50, 8, 15, 3, GUI_HALF);
    gui_draw_text(52, 9, "Button 3");
    
    // Draw some shapes
    gui_draw_circle(20, 15, 4, GUI_CIRCLE);
    gui_draw_circle(40, 15, 4, GUI_STAR);
    gui_draw_circle(60, 15, 4, GUI_BLOCK);
    
    // Info text
    gui_draw_text(8, 19, "Press any key to exit GUI demo");
    
    gui_render();
    
    // Wait for key press
    while (1) {
        poll_keyboard();
        if (has_key()) {
            getchar();
            break;
        }
    }
    
    gui_exit();
}
