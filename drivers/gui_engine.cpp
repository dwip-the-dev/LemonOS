// LemonOS C++ GUI Component & Layout Engine
// Object-oriented bounding boxes, outline hitboxes, and overflow management
// Author: Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "gui_engine.h"

extern "C" {
#include "graphics.h"
#include "font.h"
#include "icons.h"
#include "fast_mem.h"
}

void GuiBox::render_card(int radius, uint32_t bg, uint32_t border, bool is_hover) const {
    uint32_t active_bg = is_hover ? 0xFF2A2B3D : bg;
    uint32_t active_border = is_hover ? 0xFF89B4FA : border;

    gfx_fill_rounded_rect(x, y, width, height, radius, active_bg);
    gfx_draw_rounded_rect(x, y, width, height, radius, active_border);

    if (is_hover) {
        // Subtle luminous inner border
        gfx_draw_rounded_rect(x + 1, y + 1, width - 2, height - 2, radius > 1 ? radius - 1 : 1, 0x4489B4FA);
    }
}

void GuiBox::render_button(const char* label, uint32_t bg, uint32_t fg, bool is_hover) const {
    uint32_t active_bg = is_hover ? 0xFF363A4F : bg;
    uint32_t active_fg = is_hover ? 0xFFFFFFFF : fg;

    gfx_fill_rounded_rect(x, y, width, height, 6, active_bg);
    gfx_draw_rounded_rect(x, y, width, height, 6, is_hover ? 0xFF89B4FA : 0xFF45475A);

    if (!label) return;
    int lw = font_get_string_width(label);
    if (lw <= width - 8) {
        int tx = x + (width - lw) / 2;
        int ty = y + (height - FONT_HEIGHT) / 2;
        gfx_draw_text(tx, ty, label, active_fg);
    } else {
        gfx_draw_text_ellipsis(x + 4, y + (height - FONT_HEIGHT) / 2, width - 8, label, active_fg);
    }
}

void GuiLayout::clip_text_ellipsis(int x, int y, int max_w, const char* text, uint32_t color) {
    if (!text || max_w <= 0) return;
    gfx_draw_text_ellipsis(x, y, max_w, text, color);
}

void GuiLayout::clip_text(int x, int y, int max_w, const char* text, uint32_t color) {
    if (!text || max_w <= 0) return;
    gfx_draw_text_clipped(x, y, max_w, text, color);
}

// C-Linkage API Implementation
extern "C" {

int gui_hitbox_contains(int x, int y, int w, int h, int px, int py) {
    GuiBox box(x, y, w, h);
    return box.contains(px, py) ? 1 : 0;
}

void gui_render_card(int x, int y, int w, int h, int radius, uint32_t bg_color, uint32_t border_color, int is_hover) {
    GuiBox box(x, y, w, h);
    box.render_card(radius, bg_color, border_color, is_hover != 0);
}

void gui_render_button(int x, int y, int w, int h, const char* label, uint32_t bg_color, uint32_t fg_color, int is_hover) {
    GuiBox box(x, y, w, h);
    box.render_button(label, bg_color, fg_color, is_hover != 0);
}

void gui_render_text_ellipsis(int x, int y, int max_w, const char* text, uint32_t color) {
    GuiLayout::clip_text_ellipsis(x, y, max_w, text, color);
}

void gui_render_text_clipped(int x, int y, int max_w, const char* text, uint32_t color) {
    GuiLayout::clip_text(x, y, max_w, text, color);
}

void gui_get_desktop_icon_box(int ix, int iy, gui_rect_t* out_box) {
    if (!out_box) return;
    out_box->x = ix - 19;
    out_box->y = iy - 6;
    out_box->w = 86;
    out_box->h = 78;
}

void gui_render_desktop_tile(int ix, int iy, const char* label, int icon_id, int is_hover) {
    int tile_x = ix - 19;
    int tile_y = iy - 6;
    int tile_w = 86;
    int tile_h = 78;

    // 1. Draw outline box hover feedback
    if (is_hover) {
        gfx_fill_rounded_rect(tile_x, tile_y, tile_w, tile_h, 8, 0x3389B4FA);
        gfx_draw_rounded_rect(tile_x, tile_y, tile_w, tile_h, 8, 0x8889B4FA);
    }

    // 2. Center 48x48 icon backing plate
    gfx_fill_rounded_rect(ix, iy, 48, 48, 10, COLOR_SURFACE_LIGHT);
    gfx_draw_rounded_rect(ix, iy, 48, 48, 10, is_hover ? 0xFF89B4FA : COLOR_OVERLAY);
    icon_draw_32(ix + 8, iy + 8, (icon_type_t)icon_id);

    // 3. Label rendering with zero text overflow
    if (label) {
        int lw = font_get_string_width(label);
        if (lw <= tile_w - 6) {
            int lx = tile_x + (tile_w - lw) / 2;
            gfx_draw_text(lx + 1, iy + 53, label, 0xFF000000);
            gfx_draw_text(lx, iy + 52, label, is_hover ? 0xFF89B4FA : COLOR_TEXT_PRIMARY);
        } else {
            gfx_draw_text_ellipsis(tile_x + 3, iy + 52, tile_w - 6, label, is_hover ? 0xFF89B4FA : COLOR_TEXT_PRIMARY);
        }
    }
}

} // extern "C"
