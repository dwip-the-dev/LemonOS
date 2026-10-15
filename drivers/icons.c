#include "icons.h"
#include "icon_data.h"
#include "graphics.h"
#include <stdint.h>
#include <stddef.h>

static void draw_tinted_bitmap(int x, int y, int w, int h, const uint32_t* data, uint32_t tint) {
    if (!data) return;
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            uint32_t p = data[r * w + c];
            uint8_t a = (p >> 24) & 0xFF;
            if (a > 0) {
                gfx_put_pixel_alpha(x + c, y + r, tint & 0x00FFFFFF, a);
            }
        }
    }
}

// 16x16 Icons (Titlebars, Tab favicons, Top Bar, Controls)
void icon_draw_16(int x, int y, icon_type_t type, uint32_t color) {
    const uint32_t* data = NULL;
    int can_tint = 0;

    switch (type) {
        case ICON_FIREFOX:      data = icon_data_firefox_16; break;
        case ICON_TERMINAL:     data = icon_data_terminal_16; break;
        case ICON_FILE_MANAGER: data = icon_data_folder_16; break;
        case ICON_CALCULATOR:   data = icon_data_calculator_16; break;
        case ICON_CONVERTER:    data = icon_data_convert_16; break;
        case ICON_CLOCK:        data = icon_data_clock_16; break;
        case ICON_GAMES:        data = icon_data_games_16; break;
        case ICON_SETTINGS:     data = icon_data_settings_16; break;
        case ICON_SYSINFO:      data = icon_data_sysinfo_16; break;
        case ICON_MATRIX:       data = icon_data_matrix_16; break;
        case ICON_LEMON_OS:     data = icon_data_lemon_16; break;
        case ICON_POWER:        data = icon_data_power_16; can_tint = 1; break;
        case ICON_RESTART:      data = icon_data_restart_16; can_tint = 1; break;
        case ICON_SEARCH:       data = icon_data_search_16; can_tint = 1; break;
        case ICON_WIFI:         data = icon_data_wifi_16; can_tint = 1; break;
        case ICON_BATTERY:      data = icon_data_battery_16; can_tint = 1; break;
        case ICON_SPEAKER:      data = icon_data_speaker_16; can_tint = 1; break;
        case ICON_LOCK:         data = icon_data_lock_16; break;
        case ICON_GLOBE:        data = icon_data_globe_16; break;
        case ICON_HOME:         data = icon_data_home_16; break;
        case ICON_ARROW_LEFT:   data = icon_data_arrow_left_16; can_tint = 1; break;
        case ICON_ARROW_RIGHT:  data = icon_data_arrow_right_16; can_tint = 1; break;
        case ICON_REFRESH:      data = icon_data_restart_16; can_tint = 1; break;
        case ICON_CLOSE:        data = icon_data_close_16; can_tint = 1; break;
        case ICON_MINIMIZE:     data = icon_data_minimize_16; can_tint = 1; break;
        case ICON_MAXIMIZE:     data = icon_data_maximize_16; can_tint = 1; break;
        case ICON_RESTORE:      data = icon_data_restore_16; can_tint = 1; break;
        default: break;
    }

    if (!data) return;

    if (can_tint && color != 0) {
        draw_tinted_bitmap(x, y, 16, 16, data, color);
    } else {
        gfx_draw_bitmap_rgba(x, y, 16, 16, data);
    }
}

// 24x24 Icons (Floating Dock / Taskbar)
void icon_draw_24(int x, int y, icon_type_t type) {
    const uint32_t* data = NULL;

    switch (type) {
        case ICON_FIREFOX:      data = icon_data_firefox_24; break;
        case ICON_TERMINAL:     data = icon_data_terminal_24; break;
        case ICON_FILE_MANAGER: data = icon_data_folder_24; break;
        case ICON_CALCULATOR:   data = icon_data_calculator_24; break;
        case ICON_CONVERTER:    data = icon_data_convert_24; break;
        case ICON_CLOCK:        data = icon_data_clock_24; break;
        case ICON_GAMES:        data = icon_data_games_24; break;
        case ICON_SETTINGS:     data = icon_data_settings_24; break;
        case ICON_SYSINFO:      data = icon_data_sysinfo_24; break;
        case ICON_MATRIX:       data = icon_data_matrix_24; break;
        case ICON_LEMON_OS:     data = icon_data_lemon_24; break;
        case ICON_POWER:        data = icon_data_power_24; break;
        case ICON_RESTART:      data = icon_data_restart_24; break;
        case ICON_SEARCH:       data = icon_data_search_24; break;
        case ICON_WIFI:         data = icon_data_wifi_24; break;
        case ICON_BATTERY:      data = icon_data_battery_24; break;
        case ICON_SPEAKER:      data = icon_data_speaker_24; break;
        case ICON_LOCK:         data = icon_data_lock_24; break;
        case ICON_GLOBE:        data = icon_data_globe_24; break;
        case ICON_HOME:         data = icon_data_home_24; break;
        case ICON_ARROW_LEFT:   data = icon_data_arrow_left_24; break;
        case ICON_ARROW_RIGHT:  data = icon_data_arrow_right_24; break;
        case ICON_REFRESH:      data = icon_data_restart_24; break;
        case ICON_CLOSE:        data = icon_data_close_24; break;
        case ICON_MINIMIZE:     data = icon_data_minimize_24; break;
        case ICON_MAXIMIZE:     data = icon_data_maximize_24; break;
        case ICON_RESTORE:      data = icon_data_restore_24; break;
        default: break;
    }

    if (data) {
        gfx_draw_bitmap_rgba(x, y, 24, 24, data);
    }
}

// 32x32 Icons (Start Menu Grid & Desktop Icons)
void icon_draw_32(int x, int y, icon_type_t type) {
    const uint32_t* data = NULL;

    switch (type) {
        case ICON_FIREFOX:      data = icon_data_firefox_32; break;
        case ICON_TERMINAL:     data = icon_data_terminal_32; break;
        case ICON_FILE_MANAGER: data = icon_data_folder_32; break;
        case ICON_CALCULATOR:   data = icon_data_calculator_32; break;
        case ICON_CONVERTER:    data = icon_data_convert_32; break;
        case ICON_CLOCK:        data = icon_data_clock_32; break;
        case ICON_GAMES:        data = icon_data_games_32; break;
        case ICON_SETTINGS:     data = icon_data_settings_32; break;
        case ICON_SYSINFO:      data = icon_data_sysinfo_32; break;
        case ICON_MATRIX:       data = icon_data_matrix_32; break;
        case ICON_LEMON_OS:     data = icon_data_lemon_32; break;
        case ICON_POWER:        data = icon_data_power_32; break;
        case ICON_RESTART:      data = icon_data_restart_32; break;
        case ICON_SEARCH:       data = icon_data_search_32; break;
        case ICON_WIFI:         data = icon_data_wifi_32; break;
        case ICON_BATTERY:      data = icon_data_battery_32; break;
        case ICON_SPEAKER:      data = icon_data_speaker_32; break;
        case ICON_LOCK:         data = icon_data_lock_32; break;
        case ICON_GLOBE:        data = icon_data_globe_32; break;
        case ICON_HOME:         data = icon_data_home_32; break;
        case ICON_ARROW_LEFT:   data = icon_data_arrow_left_32; break;
        case ICON_ARROW_RIGHT:  data = icon_data_arrow_right_32; break;
        case ICON_REFRESH:      data = icon_data_restart_32; break;
        case ICON_CLOSE:        data = icon_data_close_32; break;
        case ICON_MINIMIZE:     data = icon_data_minimize_32; break;
        case ICON_MAXIMIZE:     data = icon_data_maximize_32; break;
        case ICON_RESTORE:      data = icon_data_restore_32; break;
        default: break;
    }

    if (data) {
        gfx_draw_bitmap_rgba(x, y, 32, 32, data);
    }
}
