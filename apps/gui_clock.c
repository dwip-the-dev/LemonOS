#include "gui_clock.h"
#include "graphics.h"
#include "font.h"
#include "rtc.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    int sw_running;
    uint32_t sw_seconds;
    uint32_t sw_ticks;
} clock_state_t;

static clock_state_t clock_state;
static window_t* clock_win = NULL;

static const char* month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static void clock_render(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win; (void)ch;
    gfx_fill_rect(cx, cy, cw, ch, COLOR_SURFACE);

    int h, m, s, day, month, year;
    get_rtc_time(&h, &m, &s);
    get_rtc_date(&day, &month, &year);

    // Update stopwatch tick
    if (clock_state.sw_running) {
        clock_state.sw_ticks++;
        if (clock_state.sw_ticks >= 30) { // ~30 frames/sec
            clock_state.sw_ticks = 0;
            clock_state.sw_seconds++;
        }
    }

    // 1. Digital Clock Display Card
    int card_w = cw - 32;
    int card_h = 70;
    int card_x = cx + 16;
    int card_y = cy + 16;

    gfx_fill_rounded_rect(card_x, card_y, card_w, card_h, 8, 0xFF181825);
    gfx_draw_rounded_rect(card_x, card_y, card_w, card_h, 8, COLOR_OVERLAY);

    // Format HH:MM:SS
    char time_buf[16];
    time_buf[0] = '0' + (h / 10);
    time_buf[1] = '0' + (h % 10);
    time_buf[2] = ':';
    time_buf[3] = '0' + (m / 10);
    time_buf[4] = '0' + (m % 10);
    time_buf[5] = ':';
    time_buf[6] = '0' + (s / 10);
    time_buf[7] = '0' + (s % 10);
    time_buf[8] = '\0';

    int tw = font_get_string_width(time_buf);
    gfx_draw_text(card_x + (card_w - tw) / 2, card_y + 14, time_buf, COLOR_LEMON_GOLD);

    // Format Date: DD Month YYYY
    char date_buf[32];
    int dp = 0;
    if (month >= 1 && month <= 12) {
        const char* mn = month_names[month - 1];
        while (*mn) date_buf[dp++] = *mn++;
    }
    date_buf[dp++] = ' ';
    date_buf[dp++] = '0' + (day / 10);
    date_buf[dp++] = '0' + (day % 10);
    date_buf[dp++] = ',';
    date_buf[dp++] = ' ';
    date_buf[dp++] = '2'; date_buf[dp++] = '0';
    date_buf[dp++] = '0' + ((year / 10) % 10);
    date_buf[dp++] = '0' + (year % 10);
    date_buf[dp] = '\0';

    int dw = font_get_string_width(date_buf);
    gfx_draw_text(card_x + (card_w - dw) / 2, card_y + 42, date_buf, COLOR_TEXT_SECONDARY);

    // 2. Stopwatch Section Card
    int sw_y = card_y + card_h + 16;
    int sw_h = 100;
    gfx_fill_rounded_rect(card_x, sw_y, card_w, sw_h, 8, 0xFF181825);
    gfx_draw_rounded_rect(card_x, sw_y, card_w, sw_h, 8, COLOR_OVERLAY);

    gfx_draw_text(card_x + 14, sw_y + 10, "Stopwatch / Timer", COLOR_TEXT_MUTED);

    // Format Stopwatch Time: MM:SS
    uint32_t sw_m = clock_state.sw_seconds / 60;
    uint32_t sw_s = clock_state.sw_seconds % 60;
    char sw_buf[16];
    sw_buf[0] = '0' + (sw_m / 10);
    sw_buf[1] = '0' + (sw_m % 10);
    sw_buf[2] = ':';
    sw_buf[3] = '0' + (sw_s / 10);
    sw_buf[4] = '0' + (sw_s % 10);
    sw_buf[5] = '\0';

    int stw = font_get_string_width(sw_buf);
    gfx_draw_text(card_x + (card_w - stw) / 2, sw_y + 30, sw_buf, COLOR_GREEN_ACCENT);

    // Buttons: [ Start / Pause ]  [ Reset ]
    int btn_w = 80;
    int btn_h = 24;
    int btn_gap = 12;
    int btn_total_w = 2 * btn_w + btn_gap;
    int b1_x = card_x + (card_w - btn_total_w) / 2;
    int b2_x = b1_x + btn_w + btn_gap;
    int btn_y = sw_y + 64;

    const char* b1_label = clock_state.sw_running ? "Pause" : "Start";
    uint32_t b1_bg = clock_state.sw_running ? COLOR_YELLOW_ACCENT : COLOR_GREEN_ACCENT;
    gfx_fill_rounded_rect(b1_x, btn_y, btn_w, btn_h, 4, b1_bg);
    int b1_w = font_get_string_width(b1_label);
    gfx_draw_text(b1_x + (btn_w - b1_w) / 2, btn_y + 4, b1_label, 0xFF11111B);

    gfx_fill_rounded_rect(b2_x, btn_y, btn_w, btn_h, 4, COLOR_OVERLAY);
    gfx_draw_rounded_rect(b2_x, btn_y, btn_w, btn_h, 4, COLOR_BORDER);
    int b2_w = font_get_string_width("Reset");
    gfx_draw_text(b2_x + (btn_w - b2_w) / 2, btn_y + 4, "Reset", COLOR_TEXT_PRIMARY);
}

static void clock_on_mouse(window_t* win, int rx, int ry, int buttons, int event_type) {
    (void)win; (void)buttons;
    if (event_type != WM_EVENT_MOUSE_CLICK) return;

    int cw = win->width - 6;
    int card_w = cw - 32;
    int card_x = 16;
    int card_h = 70;
    int sw_y = 16 + card_h + 16;

    int btn_w = 80;
    int btn_h = 24;
    int btn_gap = 12;
    int btn_total_w = 2 * btn_w + btn_gap;
    int b1_x = card_x + (card_w - btn_total_w) / 2;
    int b2_x = b1_x + btn_w + btn_gap;
    int btn_y = sw_y + 64;

    // Start / Pause button
    if (rx >= b1_x && rx < b1_x + btn_w && ry >= btn_y && ry < btn_y + btn_h) {
        clock_state.sw_running = !clock_state.sw_running;
        return;
    }

    // Reset button
    if (rx >= b2_x && rx < b2_x + btn_w && ry >= btn_y && ry < btn_y + btn_h) {
        clock_state.sw_running = 0;
        clock_state.sw_seconds = 0;
        clock_state.sw_ticks = 0;
        return;
    }
}

static void clock_on_close(window_t* win) {
    (void)win;
    clock_win = NULL;
}

void gui_clock_open(void) {
    if (clock_win && !clock_win->is_closed) {
        if (clock_win->is_minimized) wm_restore_window(clock_win);
        wm_bring_to_front(clock_win);
        return;
    }

    clock_state.sw_running = 0;
    clock_state.sw_seconds = 0;
    clock_state.sw_ticks = 0;

    clock_win = wm_create_window("Clock & Timer", ICON_CLOCK, 240, 130, 360, 260, clock_render);
    if (clock_win) {
        clock_win->on_mouse = clock_on_mouse;
        clock_win->on_close = clock_on_close;
    }
}
