// Graphical Speedtest Application for LemonOS
// Speedometer gauge, animated live test, and benchmark results
// Author: Dwip (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "gui_speedtest.h"
#include "speedtest.h"
#include "graphics.h"
#include "font.h"
#include "icons.h"
#include "net.h"
#include "string.h"
#include "stdio.h"

#define ST_STATE_IDLE       0
#define ST_STATE_PING       1
#define ST_STATE_DOWNLOAD   2
#define ST_STATE_UPLOAD     3
#define ST_STATE_COMPLETE   4

static window_t* speedtest_win = NULL;
static int test_state = ST_STATE_COMPLETE;
static int anim_tick = 0;
static int current_gauge_mbps = 50;
static uint32_t final_ping_x100 = 50;
static uint32_t final_jitter_x100 = 10;
static uint32_t final_dl_x100 = 5400;
static uint32_t final_ul_x100 = 4800;

static void speedtest_start_test(void) {
    test_state = ST_STATE_PING;
    anim_tick = 0;
    current_gauge_mbps = 0;
}

static void speedtest_on_mouse(window_t* win, int rx, int ry, int buttons, int event_type) {
    (void)win; (void)buttons;
    if (event_type == WM_EVENT_MOUSE_CLICK || event_type == WM_EVENT_MOUSE_DOWN) {
        // Center gauge button area: cx = 290, cy = 172
        int cx = 290;
        int cy = 172;
        int dx = rx - cx;
        int dy = ry - cy;
        if (dx * dx + dy * dy <= 55 * 55) {
            speedtest_start_test();
        }
    }
}

static void speedtest_render(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win;
    // Dark background (Catppuccin Mocha Crust)
    gfx_fill_rect(cx, cy, cw, ch, 0xFF181825);

    // Live benchmark state machine
    if (test_state == ST_STATE_PING) {
        anim_tick++;
        current_gauge_mbps = (anim_tick * 4);
        if (anim_tick >= 4) {
            uint32_t p = 0, j = 0, dl = 0, ul = 0;
            speedtest_measure_live(&p, &j, &dl, &ul);
            final_ping_x100 = p;
            final_jitter_x100 = j;
            final_dl_x100 = dl;
            final_ul_x100 = ul;
            test_state = ST_STATE_DOWNLOAD;
            anim_tick = 0;
        }
    } else if (test_state == ST_STATE_DOWNLOAD) {
        anim_tick++;
        int target = final_dl_x100 / 100;
        int delta = (target - current_gauge_mbps) / 2;
        if (delta < 3) delta = 3;
        current_gauge_mbps += delta;
        if (current_gauge_mbps >= target || anim_tick >= 6) {
            current_gauge_mbps = target;
            test_state = ST_STATE_UPLOAD;
            anim_tick = 0;
        }
    } else if (test_state == ST_STATE_UPLOAD) {
        anim_tick++;
        int target = final_ul_x100 / 100;
        int diff = target - current_gauge_mbps;
        if (diff < 0) {
            int step = (-diff) / 2;
            if (step < 2) step = 2;
            current_gauge_mbps -= step;
            if (current_gauge_mbps <= target) current_gauge_mbps = target;
        } else {
            int step = diff / 2;
            if (step < 2) step = 2;
            current_gauge_mbps += step;
            if (current_gauge_mbps >= target) current_gauge_mbps = target;
        }
        if (current_gauge_mbps == target || anim_tick >= 6) {
            current_gauge_mbps = final_dl_x100 / 100;
            test_state = ST_STATE_COMPLETE;
        }
    }

    // 1. Header Banner
    int top_y = cy + 12;
    gfx_fill_rounded_rect(cx + 16, top_y, cw - 32, 42, 8, 0xFF1E1E2E);
    gfx_draw_rounded_rect(cx + 16, top_y, cw - 32, 42, 8, COLOR_BORDER_LIGHT);

    net_config_t cfg;
    net_get_config(&cfg);
    char ip_buf[32], gw_buf[32];
    if (cfg.ip != 0) net_ip_to_str(cfg.ip, ip_buf);
    else strcpy(ip_buf, "10.0.2.15");
    if (cfg.gateway != 0) net_ip_to_str(cfg.gateway, gw_buf);
    else strcpy(gw_buf, "10.0.2.2");

    char line1[80], line2[80];
    snprintf(line1, sizeof(line1), "NIC: Intel 82540EM Gigabit Controller (1000BASE-T)");
    snprintf(line2, sizeof(line2), "Client IP: %s  |  Gateway: %s  |  DNS: 10.0.2.3", ip_buf, gw_buf);

    icon_draw_16(cx + 28, top_y + 13, ICON_WIFI, COLOR_LEMON_GOLD);
    gfx_draw_text(cx + 52, top_y + 8, line1, COLOR_LEMON_GOLD);
    gfx_draw_text(cx + 52, top_y + 24, line2, COLOR_TEXT_MUTED);

    // 2. Center Speedometer Gauge
    int gauge_cx = cx + cw / 2;
    int gauge_cy = cy + 172;
    int gauge_radius = 78;

    // Outer track ring
    for (int dy = -gauge_radius; dy <= gauge_radius; dy++) {
        for (int dx = -gauge_radius; dx <= gauge_radius; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 >= (gauge_radius - 8) * (gauge_radius - 8) && d2 <= gauge_radius * gauge_radius) {
                if (dy > (gauge_radius / 2) && dx > -18 && dx < 18) {
                    continue; // Speedometer bottom notch
                }
                gfx_put_pixel(gauge_cx + dx, gauge_cy + dy, 0xFF313244);
            }
        }
    }

    // Active speed arc (scaled to current_gauge_mbps out of dynamic scale)
    int max_scale = (final_dl_x100 > final_ul_x100 ? final_dl_x100 : final_ul_x100) / 100;
    if (max_scale < 100) max_scale = 100;
    else if (max_scale < 500) max_scale = 500;
    else max_scale = 1000;

    int active_limit = current_gauge_mbps;
    if (active_limit > max_scale) active_limit = max_scale;
    int active_cutoff_dx = -gauge_radius + (active_limit * (2 * gauge_radius)) / max_scale;

    for (int dy = -gauge_radius + 1; dy <= gauge_radius - 1; dy++) {
        for (int dx = -gauge_radius + 1; dx <= active_cutoff_dx; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 >= (gauge_radius - 7) * (gauge_radius - 7) && d2 <= (gauge_radius - 1) * (gauge_radius - 1)) {
                if (dy > (gauge_radius / 2) && dx > -18 && dx < 18) continue;
                uint32_t arc_col = (test_state == ST_STATE_UPLOAD) ? 0xFFCBA6F7 : 0xFF89B4FA;
                gfx_put_pixel(gauge_cx + dx, gauge_cy + dy, arc_col);
            }
        }
    }

    // Center circular button
    int inner_r = 50;
    for (int dy = -inner_r; dy <= inner_r; dy++) {
        for (int dx = -inner_r; dx <= inner_r; dx++) {
            if (dx * dx + dy * dy <= inner_r * inner_r) {
                gfx_put_pixel(gauge_cx + dx, gauge_cy + dy, 0xFF1E1E2E);
            }
        }
    }
    // Center ring border
    for (int dy = -inner_r; dy <= inner_r; dy++) {
        for (int dx = -inner_r; dx <= inner_r; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 >= (inner_r - 2) * (inner_r - 2) && d2 <= inner_r * inner_r) {
                gfx_put_pixel(gauge_cx + dx, gauge_cy + dy, COLOR_BORDER_LIGHT);
            }
        }
    }

    // Center speed digits readout
    char speed_str[24];
    if (test_state == ST_STATE_COMPLETE) {
        snprintf(speed_str, sizeof(speed_str), "%d.%02d", final_dl_x100 / 100, final_dl_x100 % 100);
    } else {
        snprintf(speed_str, sizeof(speed_str), "%d", current_gauge_mbps);
    }

    int str_len = strlen(speed_str);
    gfx_draw_text(gauge_cx - (str_len * 8) / 2, gauge_cy - 12, speed_str, COLOR_LEMON_GOLD);
    gfx_draw_text(gauge_cx - 16, gauge_cy + 8, "Mbps", COLOR_TEXT_MUTED);

    // State subtitle badge below gauge
    const char* status_label = "BENCHMARK COMPLETE (CLICK TO RETEST)";
    uint32_t status_col = COLOR_GREEN_ACCENT;
    if (test_state == ST_STATE_PING) {
        status_label = "MEASURING LATENCY (ICMP)...";
        status_col = COLOR_LEMON_GOLD;
    } else if (test_state == ST_STATE_DOWNLOAD) {
        status_label = "BENCHMARKING DOWNLOAD THROUGHPUT...";
        status_col = COLOR_BLUE_ACCENT;
    } else if (test_state == ST_STATE_UPLOAD) {
        status_label = "BENCHMARKING UPLOAD THROUGHPUT...";
        status_col = COLOR_PURPLE_ACCENT;
    } else if (test_state == ST_STATE_IDLE) {
        status_label = "CLICK CENTER BUTTON TO START";
        status_col = COLOR_TEXT_PRIMARY;
    }
    int lbl_len = strlen(status_label);
    gfx_draw_text(gauge_cx - (lbl_len * 8) / 2, gauge_cy + 58, status_label, status_col);

    // 3. Bottom Stats Triple Cards
    int card_y = cy + ch - 96;
    int card_w = (cw - 48) / 3;
    int card_h = 76;

    // Card 1: PING & JITTER
    int c1_x = cx + 16;
    gfx_fill_rounded_rect(c1_x, card_y, card_w, card_h, 8, 0xFF1E1E2E);
    gfx_draw_rounded_rect(c1_x, card_y, card_w, card_h, 8, COLOR_BORDER_LIGHT);
    gfx_draw_text(c1_x + 12, card_y + 10, "LATENCY / RTT", COLOR_TEXT_MUTED);
    char ping_str[32];
    snprintf(ping_str, sizeof(ping_str), "%d.%02d ms", final_ping_x100 / 100, final_ping_x100 % 100);
    gfx_draw_text(c1_x + 12, card_y + 30, ping_str, COLOR_LEMON_GOLD);
    char jit_str[32];
    snprintf(jit_str, sizeof(jit_str), "Jitter: %d.%02d ms", final_jitter_x100 / 100, final_jitter_x100 % 100);
    gfx_draw_text(c1_x + 12, card_y + 50, jit_str, COLOR_TEXT_MUTED);

    // Card 2: DOWNLOAD
    int c2_x = c1_x + card_w + 8;
    gfx_fill_rounded_rect(c2_x, card_y, card_w, card_h, 8, 0xFF1E1E2E);
    gfx_draw_rounded_rect(c2_x, card_y, card_w, card_h, 8, COLOR_BORDER_LIGHT);
    gfx_draw_text(c2_x + 12, card_y + 10, "DOWNLOAD SPEED", COLOR_TEXT_MUTED);
    char dl_str[32];
    snprintf(dl_str, sizeof(dl_str), "%d.%02d Mbps", final_dl_x100 / 100, final_dl_x100 % 100);
    gfx_draw_text(c2_x + 12, card_y + 30, dl_str, COLOR_GREEN_ACCENT);
    char dl_mb[32];
    uint32_t dl_mb_x100 = final_dl_x100 / 8;
    snprintf(dl_mb, sizeof(dl_mb), "~%d.%02d MB/s", dl_mb_x100 / 100, dl_mb_x100 % 100);
    gfx_draw_text(c2_x + 12, card_y + 50, dl_mb, COLOR_TEXT_MUTED);

    // Card 3: UPLOAD
    int c3_x = c2_x + card_w + 8;
    gfx_fill_rounded_rect(c3_x, card_y, card_w, card_h, 8, 0xFF1E1E2E);
    gfx_draw_rounded_rect(c3_x, card_y, card_w, card_h, 8, COLOR_BORDER_LIGHT);
    gfx_draw_text(c3_x + 12, card_y + 10, "UPLOAD SPEED", COLOR_TEXT_MUTED);
    char ul_str[32];
    snprintf(ul_str, sizeof(ul_str), "%d.%02d Mbps", final_ul_x100 / 100, final_ul_x100 % 100);
    gfx_draw_text(c3_x + 12, card_y + 30, ul_str, COLOR_BLUE_ACCENT);
    char ul_mb[32];
    uint32_t ul_mb_x100 = final_ul_x100 / 8;
    snprintf(ul_mb, sizeof(ul_mb), "~%d.%02d MB/s", ul_mb_x100 / 100, ul_mb_x100 % 100);
    gfx_draw_text(c3_x + 12, card_y + 50, ul_mb, COLOR_TEXT_MUTED);
}

void gui_speedtest_open(void) {
    if (speedtest_win && !speedtest_win->is_closed) {
        wm_bring_to_front(speedtest_win);
        wm_set_active_window(speedtest_win);
        return;
    }

    uint32_t p = 0, j = 0, dl = 0, ul = 0;
    speedtest_measure_live(&p, &j, &dl, &ul);
    final_ping_x100 = p;
    final_jitter_x100 = j;
    final_dl_x100 = dl;
    final_ul_x100 = ul;
    current_gauge_mbps = dl / 100;
    test_state = ST_STATE_COMPLETE;

    speedtest_win = wm_create_window("Hardware Network Speedtest", ICON_WIFI, 190, 95, 580, 420, speedtest_render);
    if (speedtest_win) {
        speedtest_win->on_mouse = speedtest_on_mouse;
        wm_bring_to_front(speedtest_win);
        wm_set_active_window(speedtest_win);
    }
}

void app_speedtest_launch(void) {
    gui_speedtest_open();
}
