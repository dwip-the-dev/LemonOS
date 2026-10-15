#include "gui_sysinfo.h"
#include "graphics.h"
#include "font.h"
#include "system_info.h"
#include "rtc.h"
#include <stdint.h>
#include <stddef.h>

static window_t* sysinfo_win = NULL;

static void sysinfo_render(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win; (void)ch;
    gfx_fill_rect(cx, cy, cw, ch, COLOR_SURFACE);

    // 1. Left side: Big Lemon Logo Badge
    int badge_x = cx + 24;
    int badge_y = cy + 24;
    int badge_w = 120;
    int badge_h = 140;

    gfx_fill_rounded_rect(badge_x, badge_y, badge_w, badge_h, 12, 0xFF181825);
    gfx_draw_rounded_rect(badge_x, badge_y, badge_w, badge_h, 12, COLOR_BORDER);

    // Draw large lemon emblem
    int lemon_cx = badge_x + badge_w / 2;
    int lemon_cy = badge_y + 60;
    for (int dy = -36; dy <= 36; dy++) {
        for (int dx = -36; dx <= 36; dx++) {
            if (dx * dx * 1.3 + dy * dy <= 1200) {
                gfx_put_pixel(lemon_cx + dx, lemon_cy + dy, COLOR_LEMON_GOLD);
            }
        }
    }
    // Green leaf at top
    for (int dy = -48; dy <= -32; dy++) {
        for (int dx = -5; dx <= 18; dx++) {
            if ((dx - 6) * (dx - 6) + (dy + 40) * (dy + 40) <= 64) {
                gfx_put_pixel(lemon_cx + dx, lemon_cy + dy, COLOR_GREEN_ACCENT);
            }
        }
    }

    gfx_draw_text(badge_x + 18, badge_y + 110, "LemonOS 3.0", COLOR_LEMON_GOLD);

    // 2. Right side: System Specs
    int spec_x = cx + 160;
    int cur_y = cy + 20;

    gfx_draw_text(spec_x, cur_y, "LemonOS Desktop Edition", COLOR_LEMON_GOLD);
    cur_y += 24;

    // Spec entries
    char vendor[13] = {0};
    char brand[49] = {0};
    get_cpu_vendor(vendor);
    get_cpu_brand(brand);

    typedef struct {
        const char* label;
        const char* value;
        uint32_t val_color;
    } spec_item_t;

    spec_item_t specs[] = {
        {"Kernel:", "LemonOS 32-bit x86 Protected Mode", COLOR_GREEN_ACCENT},
        {"Desktop:", "LemonDE (GNOME + Windows Hybrid)", COLOR_PURPLE_ACCENT},
        {"Display:", "1024x768 TrueColor (32 bpp, BGA)", COLOR_BLUE_ACCENT},
        {"CPU Vendor:", vendor, COLOR_TEAL_ACCENT},
        {"CPU Model:", brand[0] ? brand : "x86 Compatible Processor", COLOR_TEXT_PRIMARY},
        {"Memory:", "1024 MB (1 GB) RAM (Two-Level x86 Paging)", COLOR_LEMON_GOLD},
        {"Filesystem:", "LemonOS SimpleFS (In-Memory)", COLOR_TEXT_PRIMARY},
        {"Developer:", "Dwip (dwip-the-dev)", COLOR_LEMON_AMBER}
    };

    int max_spec_val_w = (cx + cw - 14) - (spec_x + 95);
    for (size_t i = 0; i < sizeof(specs) / sizeof(specs[0]); i++) {
        gfx_draw_text(spec_x, cur_y, specs[i].label, COLOR_TEXT_MUTED);
        if (max_spec_val_w > 10) {
            gfx_draw_text_ellipsis(spec_x + 95, cur_y, max_spec_val_w, specs[i].value, specs[i].val_color);
        }
        cur_y += 20;
    }

    // Live RTC Clock in SysInfo
    int h, m, s, day, month, year;
    get_rtc_time(&h, &m, &s);
    get_rtc_date(&day, &month, &year);

    char time_str[48];
    // Simple format
    time_str[0] = '2'; time_str[1] = '0';
    time_str[2] = '0' + ((year / 10) % 10);
    time_str[3] = '0' + (year % 10);
    time_str[4] = '-';
    time_str[5] = '0' + (month / 10);
    time_str[6] = '0' + (month % 10);
    time_str[7] = '-';
    time_str[8] = '0' + (day / 10);
    time_str[9] = '0' + (day % 10);
    time_str[10] = ' ';
    time_str[11] = '0' + (h / 10);
    time_str[12] = '0' + (h % 10);
    time_str[13] = ':';
    time_str[14] = '0' + (m / 10);
    time_str[15] = '0' + (m % 10);
    time_str[16] = ':';
    time_str[17] = '0' + (s / 10);
    time_str[18] = '0' + (s % 10);
    time_str[19] = '\0';

    gfx_draw_text(spec_x, cur_y, "System RTC:", COLOR_TEXT_MUTED);
    if (max_spec_val_w > 10) {
        gfx_draw_text_ellipsis(spec_x + 95, cur_y, max_spec_val_w, time_str, COLOR_YELLOW_ACCENT);
    }
}

static void sysinfo_on_close(window_t* win) {
    (void)win;
    sysinfo_win = NULL;
}

void gui_sysinfo_open(void) {
    if (sysinfo_win && !sysinfo_win->is_closed) {
        if (sysinfo_win->is_minimized) wm_restore_window(sysinfo_win);
        wm_bring_to_front(sysinfo_win);
        return;
    }

    sysinfo_win = wm_create_window("About LemonOS", ICON_INFO, 220, 120, 560, 270, sysinfo_render);
    if (sysinfo_win) {
        sysinfo_win->on_close = sysinfo_on_close;
    }
}
