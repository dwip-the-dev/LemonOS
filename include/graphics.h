#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stddef.h>

#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768

// Color definitions (32-bit ARGB/XRGB)
#define COLOR_RGB(r, g, b)       (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | ((uint32_t)(b)))
#define COLOR_ARGB(a, r, g, b)   (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | ((uint32_t)(b)))
#define COLOR_R(c)               (((c) >> 16) & 0xFF)
#define COLOR_G(c)               (((c) >> 8) & 0xFF)
#define COLOR_B(c)               ((c) & 0xFF)
#define COLOR_A(c)               (((c) >> 24) & 0xFF)

// Modern UI Color Palette (GNOME / Windows Fluent / Catppuccin Dark inspired)
#define COLOR_BG_DARK            0xFF11111B // Crust dark background
#define COLOR_SURFACE            0xFF1E1E2E // Base panel surface
#define COLOR_SURFACE_LIGHT      0xFF28283D // Lighter elevated panel
#define COLOR_OVERLAY            0xFF313244 // Card overlay
#define COLOR_BORDER             0xFF45475A // Border line
#define COLOR_BORDER_LIGHT       0xFF585B70 // Active border
#define COLOR_TEXT_PRIMARY       0xFFCDD6F4 // White/light text
#define COLOR_TEXT_SECONDARY     0xFFA6ADC8 // Muted grey text
#define COLOR_TEXT_MUTED         0xFF6C7086 // Dim text

// Accent colors
#define COLOR_LEMON_GOLD         0xFFF9E2AF // Lemon yellow
#define COLOR_LEMON_AMBER        0xFFF5A914 // Amber highlight
#define COLOR_BLUE_ACCENT        0xFF89B4FA // Windows/GNOME blue
#define COLOR_GREEN_ACCENT       0xFFA6E3A1 // Success green
#define COLOR_RED_ACCENT         0xFFF38BA8 // Danger/Close red
#define COLOR_YELLOW_ACCENT      0xFFF9E2AF // Warning yellow
#define COLOR_PURPLE_ACCENT      0xFFCBA6F7 // Mauve/purple
#define COLOR_TEAL_ACCENT        0xFF94E2D5 // Teal

// Core graphics functions
int  gfx_init(void);
void gfx_clear(uint32_t color);
void gfx_put_pixel(int x, int y, uint32_t color);
void gfx_put_pixel_alpha(int x, int y, uint32_t color, uint8_t alpha);
uint32_t gfx_blend(uint32_t fg, uint32_t bg, uint8_t alpha);

// Clipping functions for window manager
void gfx_set_clip(int x, int y, int w, int h);
void gfx_reset_clip(void);
void gfx_get_clip(int* x1, int* y1, int* x2, int* y2);

// Drawing primitives
void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void gfx_draw_rect(int x, int y, int w, int h, uint32_t color);
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);
void gfx_fill_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha);
void gfx_draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);
void gfx_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);
void gfx_draw_gradient_v(int x, int y, int w, int h, uint32_t top_col, uint32_t bot_col);
void gfx_draw_gradient_h(int x, int y, int w, int h, uint32_t left_col, uint32_t right_col);
void gfx_draw_shadow(int x, int y, int w, int h, int r, uint8_t max_alpha);
void gfx_blur_rect(int x, int y, int w, int h, int radius);
void gfx_acrylic_rect(int x, int y, int w, int h, int r, uint32_t tint_col, uint8_t tint_alpha);
void gfx_draw_glow_rect(int x, int y, int w, int h, int r, uint32_t glow_color, int glow_size);

// Text & Icons
void gfx_draw_text(int x, int y, const char* str, uint32_t color);
void gfx_draw_text_scaled(int x, int y, const char* str, uint32_t color, int scale);
void gfx_draw_text_clipped(int x, int y, int max_w, const char* str, uint32_t color);
void gfx_draw_text_ellipsis(int x, int y, int max_w, const char* str, uint32_t color);
void gfx_draw_text_bg(int x, int y, const char* str, uint32_t fg, uint32_t bg);
void gfx_draw_icon(int x, int y, int icon_id, uint32_t color);
void gfx_draw_cursor(int x, int y);
void gfx_draw_bitmap_rgba(int x, int y, int w, int h, const uint32_t* data);

// Buffer management
uint32_t* gfx_get_backbuffer(void);
void gfx_flip(void);

#endif
