#include "gui_services.h"
#include "graphics.h"
#include "font.h"
#include "icons.h"
#include "service.h"
#include "proc_mgr.h"
#include "init.h"
#include "string.h"
#include "alerts.h"

#define TAB_SERVICES  0
#define TAB_PROCESSES 1

typedef struct {
    int active_tab;
    int selected_service_idx;
    int selected_proc_idx;
    char status_msg[64];
} gui_services_state_t;

static gui_services_state_t state;
static window_t* services_win = NULL;

static void draw_text_num(int x, int y, uint32_t val, uint32_t color) {
    char buf[16];
    int idx = 0;
    if (val == 0) buf[idx++] = '0';
    else {
        char t[16]; int ti = 0;
        while (val > 0) { t[ti++] = '0' + (val % 10); val /= 10; }
        for (int k = ti - 1; k >= 0; k--) buf[idx++] = t[k];
    }
    buf[idx] = '\0';
    gfx_draw_text(x, y, buf, color);
}

static void services_render(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win;
    // Window background (Catppuccin Mocha Base)
    gfx_fill_rect(cx, cy, cw, ch, 0xFF1E1E2E);

    // 1. Top Tab Navigation Bar (Height: 38px)
    int nav_h = 38;
    gfx_fill_rect(cx, cy, cw, nav_h, 0xFF181825);
    gfx_fill_rect(cx, cy + nav_h - 1, cw, 1, 0xFF313244);

    // Tab 1: System Services (systemd)
    int tab1_w = 160;
    uint32_t t1_bg = (state.active_tab == TAB_SERVICES) ? 0xFF2A2B3D : 0xFF181825;
    gfx_fill_rounded_rect(cx + 10, cy + 5, tab1_w, 28, 6, t1_bg);
    icon_draw_16(cx + 18, cy + 11, ICON_SETTINGS, (state.active_tab == TAB_SERVICES) ? 0xFFF9E2AF : 0xFF6C7086);
    gfx_draw_text(cx + 38, cy + 11, "systemd Services", (state.active_tab == TAB_SERVICES) ? 0xFFCDD6F4 : 0xFF6C7086);

    // Tab 2: Processes & Tasks
    int tab2_w = 150;
    uint32_t t2_bg = (state.active_tab == TAB_PROCESSES) ? 0xFF2A2B3D : 0xFF181825;
    gfx_fill_rounded_rect(cx + 176, cy + 5, tab2_w, 28, 6, t2_bg);
    icon_draw_16(cx + 184, cy + 11, ICON_TERMINAL, (state.active_tab == TAB_PROCESSES) ? 0xFFF9E2AF : 0xFF6C7086);
    gfx_draw_text(cx + 204, cy + 11, "Process Manager", (state.active_tab == TAB_PROCESSES) ? 0xFFCDD6F4 : 0xFF6C7086);

    // System Telemetry Pill on right
    int pill_w = 260;
    int pill_x = cx + cw - pill_w - 10;
    gfx_fill_rounded_rect(pill_x, cy + 6, pill_w, 26, 6, 0xFF11111B);
    gfx_draw_rounded_rect(pill_x, cy + 6, pill_w, 26, 6, 0xFF313244);

    int total_tasks = proc_get_active_count();
    char tele_buf[64];
    tele_buf[0] = '\0';
    strcat(tele_buf, "Runlevel 5 | Tasks: ");
    draw_text_num(pill_x + 10, cy + 11, 0, 0); // dummy call
    tele_buf[20] = '\0';
    gfx_draw_text(pill_x + 12, cy + 11, "Runlevel 5", 0xFFA6E3A1);
    gfx_draw_text(pill_x + 85, cy + 11, "|", 0xFF45475A);
    gfx_draw_text(pill_x + 95, cy + 11, "Tasks: ", 0xFFCDD6F4);
    draw_text_num(pill_x + 145, cy + 11, total_tasks, 0xFFF9E2AF);
    gfx_draw_text(pill_x + 170, cy + 11, "| Mem: 8M", 0xFF89B4FA);

    int content_y = cy + nav_h;
    int content_h = ch - nav_h;
    (void)content_h;

    if (state.active_tab == TAB_SERVICES) {
        // Action Toolbar (Start, Stop, Restart, Enable)
        int tb_h = 36;
        gfx_fill_rect(cx, content_y, cw, tb_h, 0xFF181825);
        gfx_fill_rect(cx, content_y + tb_h - 1, cw, 1, 0xFF313244);

        int bx = cx + 12;
        int by = content_y + 5;
        // [Start]
        gfx_fill_rounded_rect(bx, by, 72, 26, 6, 0xFF2A2B3D);
        gfx_draw_text(bx + 16, by + 5, "Start", 0xFFA6E3A1);
        // [Stop]
        gfx_fill_rounded_rect(bx + 80, by, 72, 26, 6, 0xFF2A2B3D);
        gfx_draw_text(bx + 96, by + 5, "Stop", 0xFFF38BA8);
        // [Restart]
        gfx_fill_rounded_rect(bx + 160, by, 76, 26, 6, 0xFF2A2B3D);
        gfx_draw_text(bx + 172, by + 5, "Restart", 0xFF89B4FA);
        // [Toggle Enable]
        gfx_fill_rounded_rect(bx + 244, by, 108, 26, 6, 0xFF2A2B3D);
        gfx_draw_text(bx + 252, by + 5, "Toggle Enable", 0xFFF9E2AF);

        // Services Table
        int table_y = content_y + tb_h;
        int th_h = 24;
        gfx_fill_rect(cx, table_y, cw, th_h, 0xFF11111B);
        gfx_draw_text(cx + 14,  table_y + 4, "Service Unit", 0xFF6C7086);
        gfx_draw_text(cx + 130, table_y + 4, "Status",       0xFF6C7086);
        gfx_draw_text(cx + 250, table_y + 4, "PID",          0xFF6C7086);
        gfx_draw_text(cx + 310, table_y + 4, "Memory",       0xFF6C7086);
        gfx_draw_text(cx + 390, table_y + 4, "Description",  0xFF6C7086);

        int row_y = table_y + th_h;
        int count = service_get_count();
        int row_h = 24;

        for (int i = 0; i < count && i < 8; i++) {
            service_entry_t* s = service_get_by_index(i);
            if (!s) continue;

            uint32_t row_bg = (i == state.selected_service_idx) ? 0xFF2A2B3D :
                              (i % 2 == 0) ? 0xFF1E1E2E : 0xFF181825;
            gfx_fill_rect(cx, row_y, cw, row_h, row_bg);

            // Name
            gfx_draw_text(cx + 14, row_y + 4, s->name, 0xFFCDD6F4);

            // Status Pill
            if (s->status == SERVICE_STATUS_RUNNING) {
                gfx_fill_rounded_rect(cx + 128, row_y + 3, 105, 18, 4, 0xFF1B3322);
                gfx_fill_rounded_rect(cx + 134, row_y + 8, 6, 6, 3, 0xFFA6E3A1);
                gfx_draw_text(cx + 144, row_y + 3, "active (run)", 0xFFA6E3A1);
            } else {
                gfx_fill_rounded_rect(cx + 128, row_y + 3, 105, 18, 4, 0xFF331B22);
                gfx_fill_rounded_rect(cx + 134, row_y + 8, 6, 6, 3, 0xFFF38BA8);
                gfx_draw_text(cx + 144, row_y + 3, "inactive", 0xFFF38BA8);
            }

            // PID
            if (s->pid > 0) draw_text_num(cx + 250, row_y + 4, s->pid, 0xFFBAC2DE);
            else gfx_draw_text(cx + 250, row_y + 4, "-", 0xFF6C7086);

            // Memory
            if (s->status == SERVICE_STATUS_RUNNING) {
                draw_text_num(cx + 310, row_y + 4, s->memory_kb, 0xFFBAC2DE);
                gfx_draw_text(cx + 348, row_y + 4, "KB", 0xFF6C7086);
            } else {
                gfx_draw_text(cx + 310, row_y + 4, "0 KB", 0xFF6C7086);
            }

            // Responsive clipped description
            int desc_max_w = cw - 400;
            if (desc_max_w > 50) {
                gfx_draw_text_ellipsis(cx + 390, row_y + 4, desc_max_w, s->description, 0xFFA6ADC8);
            }

            row_y += row_h;
        }

        // Bottom Panel: Unit File & Journal Inspector
        int pane_y = row_y + 6;
        int pane_h = ch - (pane_y - cy) - 10;
        if (pane_h > 40) {
            gfx_fill_rounded_rect(cx + 10, pane_y, cw - 20, pane_h, 8, 0xFF11111B);
            gfx_draw_rounded_rect(cx + 10, pane_y, cw - 20, pane_h, 8, 0xFF313244);

            service_entry_t* sel = service_get_by_index(state.selected_service_idx);
            if (sel) {
                int ty = pane_y + 6;
                gfx_draw_text(cx + 20, ty, "Loaded Unit:", 0xFF6C7086);
                gfx_draw_text_ellipsis(cx + 115, ty, cw - 140, sel->unit_file, 0xFF89B4FA);

                ty += 15;
                gfx_draw_text(cx + 20, ty, "ExecStart:", 0xFF6C7086);
                gfx_draw_text_ellipsis(cx + 115, ty, cw - 140, sel->exec_start, 0xFFF9E2AF);

                ty += 15;
                gfx_draw_text(cx + 20, ty, "systemd Journal Log:", 0xFF6C7086);
                ty += 14;
                for (int l = 0; l < 3; l++) {
                    if (sel->recent_logs[l][0]) {
                        gfx_draw_text(cx + 30, ty, ">", 0xFFA6E3A1);
                        gfx_draw_text_ellipsis(cx + 46, ty, cw - 70, sel->recent_logs[l], 0xFFBAC2DE);
                        ty += 14;
                    }
                }
            }
        }
    } else {
        // Tab 2: Process Manager (PCB Table)
        int th_y = content_y;
        int th_h = 24;
        gfx_fill_rect(cx, th_y, cw, th_h, 0xFF11111B);
        gfx_draw_text(cx + 14,  th_y + 4, "PID",     0xFF6C7086);
        gfx_draw_text(cx + 60,  th_y + 4, "PPID",    0xFF6C7086);
        gfx_draw_text(cx + 110, th_y + 4, "User",    0xFF6C7086);
        gfx_draw_text(cx + 160, th_y + 4, "State",   0xFF6C7086);
        gfx_draw_text(cx + 220, th_y + 4, "CPU %",   0xFF6C7086);
        gfx_draw_text(cx + 280, th_y + 4, "Memory",  0xFF6C7086);
        gfx_draw_text(cx + 360, th_y + 4, "Command Line", 0xFF6C7086);

        pcb_t procs[PROC_TABLE_MAX];
        int count = proc_get_all(procs, 14);
        int row_y = th_y + th_h;
        int row_h = 24;

        for (int i = 0; i < count && i < 12; i++) {
            uint32_t row_bg = (i == state.selected_proc_idx) ? 0xFF2A2B3D :
                              (i % 2 == 0) ? 0xFF1E1E2E : 0xFF181825;
            gfx_fill_rect(cx, row_y, cw, row_h, row_bg);

            draw_text_num(cx + 14, row_y + 4, procs[i].pid, 0xFFCDD6F4);
            draw_text_num(cx + 60, row_y + 4, procs[i].ppid, 0xFF6C7086);
            gfx_draw_text(cx + 110, row_y + 4, "root", 0xFF89B4FA);

            const char* st_str = (procs[i].state == PROC_STATE_RUNNING) ? "RUNNING" :
                                 (procs[i].state == PROC_STATE_SLEEPING) ? "SLEEP" : "STOP";
            uint32_t st_col = (procs[i].state == PROC_STATE_RUNNING) ? 0xFFA6E3A1 : 0xFFBAC2DE;
            gfx_draw_text(cx + 160, row_y + 4, st_str, st_col);

            draw_text_num(cx + 220, row_y + 4, procs[i].cpu_usage_pct, 0xFFBAC2DE);
            gfx_draw_text(cx + 236, row_y + 4, "%", 0xFF6C7086);

            draw_text_num(cx + 280, row_y + 4, procs[i].mem_usage_kb, 0xFFBAC2DE);
            gfx_draw_text(cx + 322, row_y + 4, "KB", 0xFF6C7086);

            // Responsive clipped command line
            int cmd_max_w = cw - 372;
            if (cmd_max_w > 50) {
                gfx_draw_text_ellipsis(cx + 360, row_y + 4, cmd_max_w, procs[i].cmdline, 0xFFCDD6F4);
            }

            row_y += row_h;
        }

        // Bottom process actions
        int btn_y = ch - 34 + cy;
        gfx_fill_rounded_rect(cx + 12, btn_y, 172, 26, 6, 0xFFF38BA8);
        gfx_draw_text(cx + 18, btn_y + 5, "Terminate (SIGTERM)", 0xFF11111B);

        gfx_fill_rounded_rect(cx + 194, btn_y, 130, 26, 6, 0xFF313244);
        gfx_draw_text(cx + 204, btn_y + 5, "Kill (SIGKILL)", 0xFFF38BA8);
    }
}

static void services_on_mouse(window_t* win, int rx, int ry, int buttons, int event_type) {
    (void)win; (void)buttons;
    if (event_type != WM_EVENT_MOUSE_CLICK) return;

    // 1. Tab switching
    if (ry < 38) {
        if (rx >= 10 && rx < 170) {
            state.active_tab = TAB_SERVICES;
        } else if (rx >= 176 && rx < 326) {
            state.active_tab = TAB_PROCESSES;
        }
        return;
    }

    if (state.active_tab == TAB_SERVICES) {
        // Toolbar clicks
        if (ry >= 38 && ry < 74) {
            service_entry_t* sel = service_get_by_index(state.selected_service_idx);
            if (!sel) return;

            if (rx >= 12 && rx < 84) {
                // Start
                service_start(sel->name);
                alert_send("Service Started", sel->name, ICON_SETTINGS, ALERT_TYPE_SUCCESS);
            } else if (rx >= 92 && rx < 164) {
                // Stop
                service_stop(sel->name);
                alert_send("Service Stopped", sel->name, ICON_SETTINGS, ALERT_TYPE_WARNING);
            } else if (rx >= 172 && rx < 248) {
                // Restart
                service_restart(sel->name);
                alert_send("Service Restarted", sel->name, ICON_SETTINGS, ALERT_TYPE_INFO);
            } else if (rx >= 256 && rx < 356) {
                // Toggle Enable
                if (sel->enabled) service_disable(sel->name);
                else service_enable(sel->name);
            }
            return;
        }

        // Row selection
        int table_y = 38 + 36 + 24;
        if (ry >= table_y && ry < table_y + 8 * 24) {
            state.selected_service_idx = (ry - table_y) / 24;
            if (state.selected_service_idx >= service_get_count()) {
                state.selected_service_idx = service_get_count() - 1;
            }
        }
    } else {
        // Tab 2 Process Actions & Selection
        int ch = win ? win->height - 35 : 405;
        int btn_y = ch - 34;
        if (ry >= btn_y && ry < btn_y + 26) {
            pcb_t procs[PROC_TABLE_MAX];
            int count = proc_get_all(procs, PROC_TABLE_MAX);
            if (state.selected_proc_idx >= 0 && state.selected_proc_idx < count) {
                pid_t target_pid = procs[state.selected_proc_idx].pid;
                if (rx >= 12 && rx < 184) {
                    // Terminate (SIGTERM)
                    if (target_pid > 1) {
                        proc_kill(target_pid, SIGTERM);
                        alert_send("Process Terminated", procs[state.selected_proc_idx].name, ICON_TERMINAL, ALERT_TYPE_WARNING);
                    } else {
                        alert_send("Denied", "Cannot terminate PID 1 (systemd)", ICON_SETTINGS, ALERT_TYPE_ERROR);
                    }
                } else if (rx >= 194 && rx < 324) {
                    // Kill (SIGKILL)
                    if (target_pid > 1) {
                        proc_kill(target_pid, SIGKILL);
                        alert_send("Process Killed", procs[state.selected_proc_idx].name, ICON_TERMINAL, ALERT_TYPE_ERROR);
                    } else {
                        alert_send("Denied", "Cannot kill PID 1 (systemd)", ICON_SETTINGS, ALERT_TYPE_ERROR);
                    }
                }
            }
            return;
        }

        int table_y = 38 + 24;
        if (ry >= table_y && ry < table_y + 12 * 24) {
            state.selected_proc_idx = (ry - table_y) / 24;
        }
    }
}

static void services_on_key(window_t* win, char key) {
    (void)win;
    if (key == '1') {
        state.active_tab = TAB_SERVICES;
    } else if (key == '2' || key == 'p' || key == 'P') {
        state.active_tab = TAB_PROCESSES;
    } else if (key == '\t') {
        state.active_tab = (state.active_tab == TAB_SERVICES) ? TAB_PROCESSES : TAB_SERVICES;
    } else if (state.active_tab == TAB_SERVICES) {
        if (key == 'j' || key == 'J' || key == '+') {
            if (state.selected_service_idx < service_get_count() - 1) state.selected_service_idx++;
        } else if (key == 'k' || key == 'K' || key == '-') {
            if (state.selected_service_idx > 0) state.selected_service_idx--;
        } else if (key == 'r' || key == 'R') {
            service_entry_t* sel = service_get_by_index(state.selected_service_idx);
            if (sel) {
                service_restart(sel->name);
                alert_send("Service Restarted", sel->name, ICON_SETTINGS, ALERT_TYPE_INFO);
            }
        } else if (key == 'x' || key == 'X') {
            service_entry_t* sel = service_get_by_index(state.selected_service_idx);
            if (sel) {
                service_stop(sel->name);
                alert_send("Service Stopped", sel->name, ICON_SETTINGS, ALERT_TYPE_WARNING);
            }
        }
    } else if (state.active_tab == TAB_PROCESSES) {
        pcb_t procs[PROC_TABLE_MAX];
        int count = proc_get_all(procs, PROC_TABLE_MAX);
        if (key == 'j' || key == 'J' || key == '+') {
            if (state.selected_proc_idx < count - 1) state.selected_proc_idx++;
        } else if (key == 'k' || key == 'K' || key == '-') {
            if (state.selected_proc_idx > 0) state.selected_proc_idx--;
        } else if (key == 't' || key == 'T') {
            if (state.selected_proc_idx >= 0 && state.selected_proc_idx < count) {
                pid_t pid = procs[state.selected_proc_idx].pid;
                if (pid > 1) {
                    proc_kill(pid, SIGTERM);
                    alert_send("Process Terminated", procs[state.selected_proc_idx].name, ICON_TERMINAL, ALERT_TYPE_WARNING);
                }
            }
        } else if (key == 'x' || key == 'X' || key == 'k' || key == 'K') {
            if (state.selected_proc_idx >= 0 && state.selected_proc_idx < count) {
                pid_t pid = procs[state.selected_proc_idx].pid;
                if (pid > 1) {
                    proc_kill(pid, SIGKILL);
                    alert_send("Process Killed", procs[state.selected_proc_idx].name, ICON_TERMINAL, ALERT_TYPE_ERROR);
                }
            }
        }
    }
}

static void services_on_close(window_t* win) {
    (void)win;
    services_win = NULL;
}

void gui_services_open(void) {
    if (services_win && !services_win->is_closed) {
        if (services_win->is_minimized) wm_restore_window(services_win);
        wm_bring_to_front(services_win);
        return;
    }

    state.active_tab = TAB_SERVICES;
    state.selected_service_idx = 0;
    state.selected_proc_idx = 0;

    services_win = wm_create_window("Services & Task Manager", ICON_SETTINGS, 60, 60, 680, 440, services_render);
    if (services_win) {
        services_win->on_mouse = services_on_mouse;
        services_win->on_key = services_on_key;
        services_win->on_close = services_on_close;
    }
}
