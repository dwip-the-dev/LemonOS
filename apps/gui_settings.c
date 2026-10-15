#include "gui_settings.h"
#include "graphics.h"
#include "font.h"
#include "desktop.h"
#include "string.h"
#include <stdint.h>
#include <stddef.h>

static window_t* settings_win = NULL;

static const char* theme_titles[4] = {
    "Lemon Gold (Default)",
    "GNOME Dark Slate",
    "Windows 11 Bloom",
    "Emerald Cyberpunk"
};

static const uint32_t theme_preview_colors[4][2] = {
    {0xFF1E1E2E, COLOR_LEMON_GOLD},
    {0xFF181926, COLOR_PURPLE_ACCENT},
    {0xFF0F172A, COLOR_BLUE_ACCENT},
    {0xFF051408, COLOR_GREEN_ACCENT}
};

static const char* wallpaper_names[4] = {
    "Matrix Cyber (Green Rain)",
    "Retro Vaporwave (Sunset)",
    "Neon Aurora (Night Sky)",
    "Cyberpunk City (Skyline)"
};

static const char* wallpaper_keys[4] = {
    "matrix",
    "vaporwave",
    "aurora",
    "cyberpunk"
};

static const uint32_t wallpaper_colors[4] = {
    0xFFA6E3A1, // Green
    0xFFF38BA8, // Pink
    0xFF89DCEB, // Cyan
    0xFFF9E2AF  // Gold
};

static int settings_mouse_rx = -1;
static int settings_mouse_ry = -1;

static void settings_render(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win; (void)ch;
    gfx_fill_rect(cx, cy, cw, ch, COLOR_SURFACE);

    // Section 1: Themes
    gfx_draw_text(cx + 20, cy + 14, "Desktop Themes & Presets:", COLOR_LEMON_GOLD);

    int cur_theme = desktop_get_theme();
    int has_custom = desktop_has_custom_wallpaper();
    int card_w = (cw - 40 - 12) / 2;
    int card_h = 56;

    for (int i = 0; i < 4; i++) {
        int r = i / 2;
        int c = i % 2;
        int kx = cx + 20 + c * (card_w + 12);
        int ky = cy + 34 + r * (card_h + 8);
        int is_cur = (!has_custom && cur_theme == i);
        int is_hover = (settings_mouse_rx >= (20 + c * (card_w + 12)) &&
                        settings_mouse_rx < (20 + c * (card_w + 12) + card_w) &&
                        settings_mouse_ry >= (34 + r * (card_h + 8)) &&
                        settings_mouse_ry < (34 + r * (card_h + 8) + card_h));

        gfx_draw_gradient_v(kx, ky, card_w, card_h, is_hover ? 0xFF2A2B3D : theme_preview_colors[i][0], 0xFF11111B);
        uint32_t border_color = is_cur ? theme_preview_colors[i][1] : (is_hover ? 0xFF89B4FA : COLOR_BORDER);
        gfx_draw_rounded_rect(kx, ky, card_w, card_h, 8, border_color);
        if (is_cur || is_hover) {
            gfx_draw_rounded_rect(kx + 1, ky + 1, card_w - 2, card_h - 2, 8, border_color);
        }

        gfx_fill_rounded_rect(kx + 10, ky + 14, 14, 14, 7, theme_preview_colors[i][1]);
        if (is_cur) {
            gfx_draw_rounded_rect(kx + 8, ky + 12, 18, 18, 9, COLOR_TEXT_PRIMARY);
        }

        // Draw title with ellipsis so it never overflows card box
        int max_tw = card_w - 36;
        if (max_tw > 20) {
            gfx_draw_text_ellipsis(kx + 30, ky + 10, max_tw, theme_titles[i], is_cur ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY);
            gfx_draw_text_ellipsis(kx + 30, ky + 28, max_tw, is_cur ? "[Active Theme]" : (is_hover ? "Click to Apply" : "Apply Theme"), is_cur ? theme_preview_colors[i][1] : COLOR_TEXT_MUTED);
        }
    }

    // Section 2: Custom Image Wallpapers
    int wall_y = cy + 34 + 2 * (card_h + 8) + 10;
    gfx_draw_text(cx + 20, wall_y, "Custom Wallpapers (Local & Downloaded):", COLOR_LEMON_GOLD);

    for (int i = 0; i < 4; i++) {
        int r = i / 2;
        int c = i % 2;
        int kx = cx + 20 + c * (card_w + 12);
        int ky = wall_y + 20 + r * (card_h + 8);
        int is_hover = (settings_mouse_rx >= (20 + c * (card_w + 12)) &&
                        settings_mouse_rx < (20 + c * (card_w + 12) + card_w) &&
                        settings_mouse_ry >= (wall_y - cy + 20 + r * (card_h + 8)) &&
                        settings_mouse_ry < (wall_y - cy + 20 + r * (card_h + 8) + card_h));

        gfx_draw_gradient_v(kx, ky, card_w, card_h, is_hover ? 0xFF2A2B3D : 0xFF181825, 0xFF11111B);
        gfx_draw_rounded_rect(kx, ky, card_w, card_h, 8, is_hover ? wallpaper_colors[i] : COLOR_BORDER);
        if (is_hover) {
            gfx_draw_rounded_rect(kx + 1, ky + 1, card_w - 2, card_h - 2, 8, wallpaper_colors[i]);
        }
        gfx_fill_rounded_rect(kx + 10, ky + 14, 14, 14, 7, wallpaper_colors[i]);

        int max_tw = card_w - 36;
        if (max_tw > 20) {
            gfx_draw_text_ellipsis(kx + 30, ky + 10, max_tw, wallpaper_names[i], COLOR_TEXT_PRIMARY);
            gfx_draw_text_ellipsis(kx + 30, ky + 28, max_tw, is_hover ? "Click to Set" : "Set Wallpaper", wallpaper_colors[i]);
        }
    }

    // Section 3: Status Info Footer
    int info_y = wall_y + 20 + 2 * (card_h + 8) + 8;
    gfx_fill_rounded_rect(cx + 20, info_y, cw - 40, 44, 8, 0xFF181825);
    gfx_draw_rounded_rect(cx + 20, info_y, cw - 40, 44, 8, COLOR_OVERLAY);

    char cur_wp_str[100] = "Current Wallpaper: ";
    strcat(cur_wp_str, desktop_get_wallpaper());
    gfx_draw_text_clipped(cx + 32, info_y + 8, cw - 64, cur_wp_str, COLOR_GREEN_ACCENT);
    gfx_draw_text_clipped(cx + 32, info_y + 24, cw - 64, "CLI commands: 'wallpaper <path>' | 'feh <path>' | 'feh --bg-scale <path>'", COLOR_TEXT_MUTED);
}

static void settings_on_mouse(window_t* win, int rx, int ry, int buttons, int event_type) {
    (void)buttons;
    settings_mouse_rx = rx;
    settings_mouse_ry = ry;

    if (event_type != WM_EVENT_MOUSE_CLICK) return;

    int cw = win->width - 6;
    int card_w = (cw - 40 - 12) / 2;
    int card_h = 56;

    // Check Theme Clicks (Entire outline box / card area)
    for (int i = 0; i < 4; i++) {
        int r = i / 2;
        int c = i % 2;
        int kx = 20 + c * (card_w + 12);
        int ky = 34 + r * (card_h + 8);

        if (rx >= kx && rx < kx + card_w && ry >= ky && ry < ky + card_h) {
            desktop_set_theme(i);
            return;
        }
    }

    // Check Wallpaper Clicks (Entire outline box / card area)
    int wall_y = 34 + 2 * (card_h + 8) + 10;
    for (int i = 0; i < 4; i++) {
        int r = i / 2;
        int c = i % 2;
        int kx = 20 + c * (card_w + 12);
        int ky = wall_y + 20 + r * (card_h + 8);

        if (rx >= kx && rx < kx + card_w && ry >= ky && ry < ky + card_h) {
            desktop_set_wallpaper(wallpaper_keys[i]);
            return;
        }
    }
}

static void settings_on_close(window_t* win) {
    (void)win;
    settings_win = NULL;
}

void gui_settings_open(void) {
    if (settings_win && !settings_win->is_closed) {
        if (settings_win->is_minimized) wm_restore_window(settings_win);
        wm_bring_to_front(settings_win);
        return;
    }

    settings_win = wm_create_window("Settings & Themes", ICON_SETTINGS, 200, 70, 540, 390, settings_render);
    if (settings_win) {
        settings_win->on_mouse = settings_on_mouse;
        settings_win->on_close = settings_on_close;
    }
}
