#ifndef GUI_H
#define GUI_H

// Use text mode graphics (80x25 characters)
#define GUI_WIDTH 80
#define GUI_HEIGHT 25

// Unique pixel types for GUI (different from text_graphics)
typedef enum {
    GUI_EMPTY = ' ',
    GUI_SOLID = '@',      // Different character
    GUI_FILL = 0xDB,      // █
    GUI_HALF = 0xB0,      // ░
    GUI_DOT = '+',        // Different character
    GUI_CIRCLE = 'O',
    GUI_STAR = '*',
    GUI_LINE = '=',
    GUI_BLOCK = 0xFE      // ■
} gui_pixel_t;

void gui_init(void);
void gui_exit(void);
void gui_draw_pixel(int x, int y, gui_pixel_t pixel);
void gui_draw_rect(int x, int y, int width, int height, gui_pixel_t pixel);
void gui_draw_line(int x1, int y1, int x2, int y2, gui_pixel_t pixel);
void gui_draw_circle(int cx, int cy, int radius, gui_pixel_t pixel);
void gui_draw_text(int x, int y, const char* text);
void gui_clear_screen(void);
void gui_render(void);
void gui_demo(void);

#endif
