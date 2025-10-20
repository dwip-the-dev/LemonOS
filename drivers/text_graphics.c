#include "text_graphics.h"
#include "vga.h"
#include "keyboard.h"

static char text_buffer[TEXT_HEIGHT][TEXT_WIDTH];
static int graphics_initialized = 0;

void textgfx_init(void) {
    if (graphics_initialized) return;
    
    for (int y = 0; y < TEXT_HEIGHT; y++) {
        for (int x = 0; x < TEXT_WIDTH; x++) {
            text_buffer[y][x] = PIXEL_EMPTY;
        }
    }
    graphics_initialized = 1;
}

void textgfx_clear(void) {
    for (int y = 0; y < TEXT_HEIGHT; y++) {
        for (int x = 0; x < TEXT_WIDTH; x++) {
            text_buffer[y][x] = PIXEL_EMPTY;
        }
    }
}

void textgfx_set_pixel(int x, int y, text_pixel_t pixel) {
    if (x >= 0 && x < TEXT_WIDTH && y >= 0 && y < TEXT_HEIGHT) {
        text_buffer[y][x] = pixel;
    }
}

void textgfx_draw_rect(int x, int y, int width, int height, text_pixel_t pixel) {
    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            textgfx_set_pixel(x + dx, y + dy, pixel);
        }
    }
}

void textgfx_draw_text(int x, int y, const char* text) {
    int pos = 0;
    while (text[pos] && (x + pos) < TEXT_WIDTH) {
        textgfx_set_pixel(x + pos, y, text[pos]);
        pos++;
    }
}

void textgfx_render(void) {
    vga_clear();
    
    for (int y = 0; y < TEXT_HEIGHT; y++) {
        for (int x = 0; x < TEXT_WIDTH; x++) {
            vga_putchar(text_buffer[y][x]);
        }
        vga_putchar('\n');
    }
}
