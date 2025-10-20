#ifndef TEXT_GRAPHICS_H
#define TEXT_GRAPHICS_H

#define TEXT_WIDTH 80
#define TEXT_HEIGHT 25

// Use ONLY basic ASCII characters that works bruhhh obiviously lol
typedef enum {
    PIXEL_EMPTY = ' ',
    PIXEL_SOLID = '#',  // Wall - basic ASCII
    PIXEL_SNAKE = 'O',  // Snake body - basic ASCII  
    PIXEL_FOOD = '@'    // Food - basic ASCII
} text_pixel_t;

void textgfx_init(void);
void textgfx_clear(void);
void textgfx_set_pixel(int x, int y, text_pixel_t pixel);
void textgfx_draw_rect(int x, int y, int width, int height, text_pixel_t pixel);
void textgfx_draw_text(int x, int y, const char* text);
void textgfx_render(void);

#endif
