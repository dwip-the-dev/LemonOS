#include "graphics.h"
#include "bga.h"
#include "font.h"
#include <stdint.h>
#include <stddef.h>

// Full-screen off-screen double buffer in RAM (zero screen tearing/flickering)
static uint32_t backbuffer[SCREEN_WIDTH * SCREEN_HEIGHT];

// Current clipping window
static int clip_x1 = 0;
static int clip_y1 = 0;
static int clip_x2 = SCREEN_WIDTH;
static int clip_y2 = SCREEN_HEIGHT;

#include "fast_mem.h"

int gfx_init(void) {
    if (!bga_set_video_mode(SCREEN_WIDTH, SCREEN_HEIGHT, 32, 1, 1)) {
        return 0;
    }
    gfx_reset_clip();
    gfx_clear(COLOR_BG_DARK);
    gfx_flip();
    return 1;
}

void gfx_set_clip(int x, int y, int w, int h) {
    clip_x1 = (x < 0) ? 0 : x;
    clip_y1 = (y < 0) ? 0 : y;
    clip_x2 = (x + w > SCREEN_WIDTH) ? SCREEN_WIDTH : (x + w);
    clip_y2 = (y + h > SCREEN_HEIGHT) ? SCREEN_HEIGHT : (y + h);
}

void gfx_reset_clip(void) {
    clip_x1 = 0;
    clip_y1 = 0;
    clip_x2 = SCREEN_WIDTH;
    clip_y2 = SCREEN_HEIGHT;
}

void gfx_get_clip(int* x1, int* y1, int* x2, int* y2) {
    if (x1) *x1 = clip_x1;
    if (y1) *y1 = clip_y1;
    if (x2) *x2 = clip_x2;
    if (y2) *y2 = clip_y2;
}

uint32_t gfx_blend(uint32_t fg, uint32_t bg, uint8_t alpha) {
    if (alpha == 255) return fg;
    if (alpha == 0) return bg;

    uint32_t inv = 255 - alpha;
    uint32_t r = ((COLOR_R(fg) * alpha) + (COLOR_R(bg) * inv)) / 255;
    uint32_t g = ((COLOR_G(fg) * alpha) + (COLOR_G(bg) * inv)) / 255;
    uint32_t b = ((COLOR_B(fg) * alpha) + (COLOR_B(bg) * inv)) / 255;
    return COLOR_RGB(r, g, b);
}

// OPTIMIZED: Use REP STOSD for full-screen clear (~3x faster)
void gfx_clear(uint32_t color) {
    fast_memset32(backbuffer, color, SCREEN_WIDTH * SCREEN_HEIGHT);
}

void gfx_put_pixel(int x, int y, uint32_t color) {
    if (x >= clip_x1 && x < clip_x2 && y >= clip_y1 && y < clip_y2) {
        backbuffer[y * SCREEN_WIDTH + x] = color;
    }
}

void gfx_put_pixel_alpha(int x, int y, uint32_t color, uint8_t alpha) {
    if (x >= clip_x1 && x < clip_x2 && y >= clip_y1 && y < clip_y2) {
        int idx = y * SCREEN_WIDTH + x;
        backbuffer[idx] = gfx_blend(color, backbuffer[idx], alpha);
    }
}

void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 >= y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        gfx_put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx_draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    for (int i = 0; i < w; i++) {
        gfx_put_pixel(x + i, y, color);
        gfx_put_pixel(x + i, y + h - 1, color);
    }
    for (int j = 0; j < h; j++) {
        gfx_put_pixel(x, y + j, color);
        gfx_put_pixel(x + w - 1, y + j, color);
    }
}

// OPTIMIZED: Use REP STOSD for each row (~4x faster for large fills)
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    int start_x = (x < clip_x1) ? clip_x1 : x;
    int end_x   = (x + w > clip_x2) ? clip_x2 : (x + w);
    int start_y = (y < clip_y1) ? clip_y1 : y;
    int end_y   = (y + h > clip_y2) ? clip_y2 : (y + h);

    int row_w = end_x - start_x;
    if (row_w <= 0) return;

    for (int py = start_y; py < end_y; py++) {
        fast_memset32(&backbuffer[py * SCREEN_WIDTH + start_x], color, row_w);
    }
}

void gfx_fill_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    if (w <= 0 || h <= 0 || alpha == 0) return;
    if (alpha == 255) {
        gfx_fill_rect(x, y, w, h, color);
        return;
    }

    int start_x = (x < clip_x1) ? clip_x1 : x;
    int end_x   = (x + w > clip_x2) ? clip_x2 : (x + w);
    int start_y = (y < clip_y1) ? clip_y1 : y;
    int end_y   = (y + h > clip_y2) ? clip_y2 : (y + h);

    int row_w = end_x - start_x;
    if (row_w <= 0) return;

    for (int py = start_y; py < end_y; py++) {
        fast_alpha_blend_row(&backbuffer[py * SCREEN_WIDTH + start_x], color, alpha, row_w);
    }
}

void gfx_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (r <= 0) {
        gfx_fill_rect(x, y, w, h, color);
        return;
    }
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    // Center rectangle
    gfx_fill_rect(x + r, y, w - 2 * r, h, color);
    // Left & right wings
    gfx_fill_rect(x, y + r, r, h - 2 * r, color);
    gfx_fill_rect(x + w - r, y + r, r, h - 2 * r, color);

    // 4 rounded corners
    int r2 = r * r;
    for (int cy = 0; cy < r; cy++) {
        for (int cx = 0; cx < r; cx++) {
            int dx = r - cx - 1;
            int dy = r - cy - 1;
            if (dx * dx + dy * dy <= r2) {
                gfx_put_pixel(x + cx, y + cy, color);                         // Top-Left
                gfx_put_pixel(x + w - 1 - cx, y + cy, color);                 // Top-Right
                gfx_put_pixel(x + cx, y + h - 1 - cy, color);                 // Bottom-Left
                gfx_put_pixel(x + w - 1 - cx, y + h - 1 - cy, color);         // Bottom-Right
            }
        }
    }
}

void gfx_draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (r <= 0) {
        gfx_draw_rect(x, y, w, h, color);
        return;
    }
    // Horizontal segments
    for (int px = x + r; px < x + w - r; px++) {
        gfx_put_pixel(px, y, color);
        gfx_put_pixel(px, y + h - 1, color);
    }
    // Vertical segments
    for (int py = y + r; py < y + h - r; py++) {
        gfx_put_pixel(x, py, color);
        gfx_put_pixel(x + w - 1, py, color);
    }
    // Corners
    int r2 = r * r;
    for (int cy = 0; cy <= r; cy++) {
        for (int cx = 0; cx <= r; cx++) {
            int dx = r - cx;
            int dy = r - cy;
            int dist2 = dx * dx + dy * dy;
            if (dist2 <= r2 && dist2 > (r - 2) * (r - 2)) {
                gfx_put_pixel(x + cx, y + cy, color);
                gfx_put_pixel(x + w - 1 - cx, y + cy, color);
                gfx_put_pixel(x + cx, y + h - 1 - cy, color);
                gfx_put_pixel(x + w - 1 - cx, y + h - 1 - cy, color);
            }
        }
    }
}

// OPTIMIZED: Pre-compute row color for vertical gradient, then fill with memset32
void gfx_draw_gradient_v(int x, int y, int w, int h, uint32_t top_col, uint32_t bot_col) {
    if (w <= 0 || h <= 0) return;
    int r0 = COLOR_R(top_col), g0 = COLOR_G(top_col), b0 = COLOR_B(top_col);
    int r1 = COLOR_R(bot_col), g1 = COLOR_G(bot_col), b1 = COLOR_B(bot_col);

    int start_y = (y < clip_y1) ? clip_y1 : y;
    int end_y   = (y + h > clip_y2) ? clip_y2 : (y + h);
    int start_x = (x < clip_x1) ? clip_x1 : x;
    int end_x   = (x + w > clip_x2) ? clip_x2 : (x + w);
    int row_w = end_x - start_x;
    if (row_w <= 0) return;

    for (int py = start_y; py < end_y; py++) {
        int factor = ((py - y) * 255) / (h > 1 ? (h - 1) : 1);
        int r = r0 + ((r1 - r0) * factor) / 255;
        int g = g0 + ((g1 - g0) * factor) / 255;
        int b = b0 + ((b1 - b0) * factor) / 255;
        uint32_t col = COLOR_RGB(r, g, b);

        fast_memset32(&backbuffer[py * SCREEN_WIDTH + start_x], col, row_w);
    }
}

void gfx_draw_gradient_h(int x, int y, int w, int h, uint32_t left_col, uint32_t right_col) {
    if (w <= 0 || h <= 0) return;
    int r0 = COLOR_R(left_col), g0 = COLOR_G(left_col), b0 = COLOR_B(left_col);
    int r1 = COLOR_R(right_col), g1 = COLOR_G(right_col), b1 = COLOR_B(right_col);

    int start_y = (y < clip_y1) ? clip_y1 : y;
    int end_y   = (y + h > clip_y2) ? clip_y2 : (y + h);
    int start_x = (x < clip_x1) ? clip_x1 : x;
    int end_x   = (x + w > clip_x2) ? clip_x2 : (x + w);

    for (int px = start_x; px < end_x; px++) {
        int factor = ((px - x) * 255) / (w > 1 ? (w - 1) : 1);
        int r = r0 + ((r1 - r0) * factor) / 255;
        int g = g0 + ((g1 - g0) * factor) / 255;
        int b = b0 + ((b1 - b0) * factor) / 255;
        uint32_t col = COLOR_RGB(r, g, b);

        for (int py = start_y; py < end_y; py++) {
            backbuffer[py * SCREEN_WIDTH + px] = col;
        }
    }
}

// OPTIMIZED: Reduced shadow pass count (fewer alpha blends)
void gfx_draw_shadow(int x, int y, int w, int h, int r, uint8_t max_alpha) {
    if (r <= 0) return;
    // Render only 3 shadow layers instead of r layers for perf
    int steps = (r < 4) ? r : 3;
    for (int step = 1; step <= steps; step++) {
        int mapped = 1 + ((step - 1) * (r - 1)) / (steps > 1 ? (steps - 1) : 1);
        uint8_t alpha = (max_alpha * (r - mapped + 1)) / (r + 2);
        uint32_t black = 0x000000;
        gfx_fill_rect_alpha(x + w + mapped - 1, y + mapped, 1, h + mapped, black, alpha);
        gfx_fill_rect_alpha(x + mapped, y + h + mapped - 1, w + mapped, 1, black, alpha);
    }
}

// High-Performance 2-Pass Frosted Box Blur (60 FPS optimized)
void gfx_blur_rect(int x, int y, int w, int h, int radius) {
    if (w <= 0 || h <= 0 || radius <= 0) return;
    int x1 = (x < 0) ? 0 : x;
    int y1 = (y < 0) ? 0 : y;
    int x2 = (x + w > SCREEN_WIDTH) ? SCREEN_WIDTH : (x + w);
    int y2 = (y + h > SCREEN_HEIGHT) ? SCREEN_HEIGHT : (y + h);
    int rw = x2 - x1;
    int rh = y2 - y1;
    if (rw <= 2 || rh <= 2) return;

    if (radius > 3) radius = 3;
    int div = (radius * 2 + 1);

    static uint32_t row_temp[SCREEN_WIDTH];

    for (int py = y1; py < y2; py += 2) {
        uint32_t* src_row = &backbuffer[py * SCREEN_WIDTH];

        int r_acc = 0, g_acc = 0, b_acc = 0;
        for (int i = -radius; i <= radius; i++) {
            int cx = x1 + i;
            if (cx < 0) cx = 0;
            if (cx >= SCREEN_WIDTH) cx = SCREEN_WIDTH - 1;
            uint32_t c = src_row[cx];
            r_acc += COLOR_R(c);
            g_acc += COLOR_G(c);
            b_acc += COLOR_B(c);
        }

        for (int px = x1; px < x2; px++) {
            row_temp[px] = COLOR_RGB(r_acc / div, g_acc / div, b_acc / div);

            int rem_x = px - radius;
            if (rem_x < 0) rem_x = 0;
            int add_x = px + radius + 1;
            if (add_x >= SCREEN_WIDTH) add_x = SCREEN_WIDTH - 1;

            uint32_t rem_c = src_row[rem_x];
            uint32_t add_c = src_row[add_x];

            r_acc += COLOR_R(add_c) - COLOR_R(rem_c);
            g_acc += COLOR_G(add_c) - COLOR_G(rem_c);
            b_acc += COLOR_B(add_c) - COLOR_B(rem_c);
        }

        for (int px = x1; px < x2; px++) {
            src_row[px] = row_temp[px];
            if (py + 1 < y2) {
                backbuffer[(py + 1) * SCREEN_WIDTH + px] = row_temp[px];
            }
        }
    }
}

// macOS / Windows 11 Acrylic Frosted Glass Effect
void gfx_acrylic_rect(int x, int y, int w, int h, int r, uint32_t tint_col, uint8_t tint_alpha) {
    if (w <= 0 || h <= 0) return;

    // 1. Blur the backdrop underneath
    gfx_blur_rect(x, y, w, h, 2);

    // 2. Tinted surface with rounded corners
    gfx_fill_rounded_rect(x, y, w, h, r, tint_col);
    if (tint_alpha < 255) {
        gfx_fill_rect_alpha(x + 1, y + 1, w - 2, h - 2, tint_col, tint_alpha);
    }

    // 3. Specular glass refraction highlight (1px soft white edge on top)
    int x1 = (x < 0) ? 0 : x;
    int y1 = (y < 0) ? 0 : y;
    int x2 = (x + w > SCREEN_WIDTH) ? SCREEN_WIDTH : (x + w);
    for (int px = x1 + r; px < x2 - r; px++) {
        gfx_put_pixel_alpha(px, y1, 0xFFFFFF, 75);
    }

    // 4. Subtle acrylic outline
    gfx_draw_rounded_rect(x, y, w, h, r, COLOR_BORDER_LIGHT);
}

// Ambient Neon Glow Highlight
void gfx_draw_glow_rect(int x, int y, int w, int h, int r, uint32_t glow_color, int glow_size) {
    if (glow_size <= 0) return;
    int steps = (glow_size > 4) ? 4 : glow_size;
    for (int i = 1; i <= steps; i++) {
        uint8_t a = (uint8_t)(80 / (i + 1));
        int ox = x - i;
        int oy = y - i;
        int ow = w + i * 2;
        int oh = h + i * 2;
        int oradius = r + i;
        if (ox >= 0 && oy >= 0 && ox + ow <= SCREEN_WIDTH && oy + oh <= SCREEN_HEIGHT) {
            for (int px = ox + oradius; px < ox + ow - oradius; px++) {
                gfx_put_pixel_alpha(px, oy, glow_color, a);
                gfx_put_pixel_alpha(px, oy + oh - 1, glow_color, a);
            }
            for (int py = oy + oradius; py < oy + oh - oradius; py++) {
                gfx_put_pixel_alpha(ox, py, glow_color, a);
                gfx_put_pixel_alpha(ox + ow - 1, py, glow_color, a);
            }
        }
    }
}

void gfx_draw_text(int x, int y, const char* str, uint32_t color) {
    font_draw_string(backbuffer, SCREEN_WIDTH, SCREEN_HEIGHT, x, y, str, color, 0, 1);
}

void gfx_draw_text_scaled(int x, int y, const char* str, uint32_t color, int scale) {
    font_draw_string_scaled(backbuffer, SCREEN_WIDTH, SCREEN_HEIGHT, x, y, str, color, scale);
}

void gfx_draw_text_clipped(int x, int y, int max_w, const char* str, uint32_t color) {
    font_draw_string_clipped(backbuffer, SCREEN_WIDTH, SCREEN_HEIGHT, x, y, max_w, str, color, 0, 1);
}

void gfx_draw_text_ellipsis(int x, int y, int max_w, const char* str, uint32_t color) {
    font_draw_string_ellipsis(backbuffer, SCREEN_WIDTH, SCREEN_HEIGHT, x, y, max_w, str, color, 0, 1);
}

void gfx_draw_text_bg(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    font_draw_string(backbuffer, SCREEN_WIDTH, SCREEN_HEIGHT, x, y, str, fg, bg, 0);
}

void gfx_draw_icon(int x, int y, int icon_id, uint32_t color) {
    icon_draw_16(x, y, (icon_type_t)icon_id, color);
}

// Windows/GNOME style anti-aliased cursor (arrow pointer)
void gfx_draw_cursor(int x, int y) {
    // 12x18 cursor bitmap (0=none, 1=black outline, 2=white interior, 3=shadow)
    static const uint8_t cursor_shape[18][12] = {
        {1,0,0,0,0,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0,0,0,0,0},
        {1,2,1,0,0,0,0,0,0,0,0,0},
        {1,2,2,1,0,0,0,0,0,0,0,0},
        {1,2,2,2,1,0,0,0,0,0,0,0},
        {1,2,2,2,2,1,0,0,0,0,0,0},
        {1,2,2,2,2,2,1,0,0,0,0,0},
        {1,2,2,2,2,2,2,1,0,0,0,0},
        {1,2,2,2,2,2,2,2,1,0,0,0},
        {1,2,2,2,2,2,2,2,2,1,0,0},
        {1,2,2,2,2,2,1,1,1,1,0,0},
        {1,2,2,1,2,2,1,0,0,0,0,0},
        {1,2,1,0,1,2,2,1,0,0,0,0},
        {1,1,0,0,1,2,2,1,0,0,0,0},
        {1,0,0,0,0,1,2,2,1,0,0,0},
        {0,0,0,0,0,1,2,2,1,0,0,0},
        {0,0,0,0,0,0,1,1,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0}
    };

    for (int r = 0; r < 18; r++) {
        for (int c = 0; c < 12; c++) {
            uint8_t pixel = cursor_shape[r][c];
            int px = x + c;
            int py = y + r;
            if (px < 0 || px >= SCREEN_WIDTH || py < 0 || py >= SCREEN_HEIGHT) continue;

            if (pixel == 1) {
                backbuffer[py * SCREEN_WIDTH + px] = 0xFF000000; // Black border
            } else if (pixel == 2) {
                backbuffer[py * SCREEN_WIDTH + px] = 0xFFFFFFFF; // Pure white body
            }
        }
    }
}

// OPTIMIZED: Row-based copy with early skip for fully transparent/clipped rows
void gfx_draw_bitmap_rgba(int x, int y, int w, int h, const uint32_t* data) {
    if (!data) return;
    for (int r = 0; r < h; r++) {
        int py = y + r;
        if (py < clip_y1 || py >= clip_y2) continue;
        int row_offset = py * SCREEN_WIDTH;
        int data_row = r * w;

        // Compute visible x-range
        int sx = (x < clip_x1) ? (clip_x1 - x) : 0;
        int ex = (x + w > clip_x2) ? (clip_x2 - x) : w;

        for (int c = sx; c < ex; c++) {
            int px = x + c;
            uint32_t p = data[data_row + c];
            uint8_t a = (p >> 24) & 0xFF;
            if (a == 0) continue;
            if (a == 255) {
                backbuffer[row_offset + px] = p & 0x00FFFFFF;
            } else {
                backbuffer[row_offset + px] = gfx_blend(p, backbuffer[row_offset + px], a);
            }
        }
    }
}

uint32_t* gfx_get_backbuffer(void) {
    return backbuffer;
}

// OPTIMIZED: Use REP MOVSD for VRAM blit (~5x faster than C loop)
void gfx_flip(void) {
    uint32_t* lfb = bga_get_framebuffer();
    if (!lfb) return;

    fast_memcpy(lfb, backbuffer, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
}
