#include "gui_calc.h"
#include "graphics.h"
#include "font.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    char display[32];
    int display_len;
    double operand_a;
    char op;
    int reset_on_next;
} calc_state_t;

static calc_state_t calc_state;
static window_t* calc_win = NULL;

static double calc_parse_double(const char* s) {
    double res = 0.0;
    double frac = 1.0;
    int in_frac = 0;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    while (*s) {
        if (*s == '.') {
            in_frac = 1;
        } else if (*s >= '0' && *s <= '9') {
            if (in_frac) {
                frac *= 0.1;
                res += (*s - '0') * frac;
            } else {
                res = res * 10.0 + (*s - '0');
            }
        }
        s++;
    }
    return res * sign;
}

static void calc_format_double(double val, char* out) {
    if (val < 0) {
        *out++ = '-';
        val = -val;
    }
    int int_part = (int)val;
    double frac_part = val - int_part;

    // Convert int part
    char temp[16];
    int p = 0;
    if (int_part == 0) {
        temp[p++] = '0';
    } else {
        while (int_part > 0) {
            temp[p++] = '0' + (int_part % 10);
            int_part /= 10;
        }
    }
    while (p > 0) {
        *out++ = temp[--p];
    }

    // Convert fraction (up to 2 decimals)
    int frac_int = (int)(frac_part * 100.0 + 0.5);
    if (frac_int > 0) {
        *out++ = '.';
        if (frac_int < 10) {
            *out++ = '0';
            *out++ = '0' + frac_int;
        } else {
            *out++ = '0' + (frac_int / 10);
            if (frac_int % 10 != 0) {
                *out++ = '0' + (frac_int % 10);
            }
        }
    }
    *out = '\0';
}

static void calc_input_char(char c) {
    if (c == 'C' || c == 'c') {
        calc_state.display[0] = '0';
        calc_state.display[1] = '\0';
        calc_state.display_len = 1;
        calc_state.operand_a = 0;
        calc_state.op = 0;
        calc_state.reset_on_next = 0;
        return;
    }

    if (c == '\b') {
        if (calc_state.display_len > 1) {
            calc_state.display_len--;
            calc_state.display[calc_state.display_len] = '\0';
        } else {
            calc_state.display[0] = '0';
            calc_state.display[1] = '\0';
            calc_state.display_len = 1;
        }
        return;
    }

    if (c == '+' || c == '-' || c == '*' || c == '/') {
        calc_state.operand_a = calc_parse_double(calc_state.display);
        calc_state.op = c;
        calc_state.reset_on_next = 1;
        return;
    }

    if (c == '=' || c == '\n') {
        if (calc_state.op != 0) {
            double b = calc_parse_double(calc_state.display);
            double res = 0.0;
            if (calc_state.op == '+') res = calc_state.operand_a + b;
            else if (calc_state.op == '-') res = calc_state.operand_a - b;
            else if (calc_state.op == '*') res = calc_state.operand_a * b;
            else if (calc_state.op == '/') {
                if (b != 0) res = calc_state.operand_a / b;
                else {
                    calc_state.display[0] = 'E';
                    calc_state.display[1] = 'r';
                    calc_state.display[2] = 'r';
                    calc_state.display[3] = '\0';
                    calc_state.display_len = 3;
                    calc_state.reset_on_next = 1;
                    calc_state.op = 0;
                    return;
                }
            }
            calc_format_double(res, calc_state.display);
            int len = 0;
            while (calc_state.display[len]) len++;
            calc_state.display_len = len;
            calc_state.op = 0;
            calc_state.reset_on_next = 1;
        }
        return;
    }

    if ((c >= '0' && c <= '9') || c == '.') {
        if (calc_state.reset_on_next || (calc_state.display_len == 1 && calc_state.display[0] == '0' && c != '.')) {
            calc_state.display[0] = c;
            calc_state.display[1] = '\0';
            calc_state.display_len = 1;
            calc_state.reset_on_next = 0;
        } else if (calc_state.display_len < 20) {
            // Only allow one decimal point
            if (c == '.') {
                for (int i = 0; i < calc_state.display_len; i++) {
                    if (calc_state.display[i] == '.') return;
                }
            }
            calc_state.display[calc_state.display_len++] = c;
            calc_state.display[calc_state.display_len] = '\0';
        }
    }
}

// 4 columns x 5 rows button matrix
static const char* calc_buttons[5][4] = {
    {"C", "(", ")", "/"},
    {"7", "8", "9", "*"},
    {"4", "5", "6", "-"},
    {"1", "2", "3", "+"},
    {"0", ".", "DEL", "="}
};

static int calc_mouse_rx = -1;
static int calc_mouse_ry = -1;

static void calc_render(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win;
    // Dark calculator background
    gfx_fill_rect(cx, cy, cw, ch, COLOR_SURFACE);

    // 1. LCD Display Screen
    int disp_x = cx + 12;
    int disp_y = cy + 12;
    int disp_w = cw - 24;
    int disp_h = 56;
    gfx_fill_rounded_rect(disp_x, disp_y, disp_w, disp_h, 8, 0xFF181825);
    gfx_draw_rounded_rect(disp_x, disp_y, disp_w, disp_h, 8, COLOR_OVERLAY);

    // Operator indicator
    if (calc_state.op) {
        char op_str[2] = {calc_state.op, '\0'};
        gfx_draw_text(disp_x + 12, disp_y + 10, op_str, COLOR_YELLOW_ACCENT);
    }

    // Number display strictly clipped to display boundaries
    int text_w = font_get_string_width(calc_state.display);
    int text_x = disp_x + disp_w - text_w - 14;
    if (text_x < disp_x + 10) text_x = disp_x + 10;
    int max_disp_text_w = disp_w - (text_x - disp_x) - 10;
    if (max_disp_text_w > 0) {
        gfx_draw_text_clipped(text_x, disp_y + 20, max_disp_text_w, calc_state.display, COLOR_GREEN_ACCENT);
    }

    // 2. Buttons Grid
    int btn_start_y = disp_y + disp_h + 14;
    int btn_area_w = cw - 24;
    int btn_gap = 8;
    int btn_w = (btn_area_w - 3 * btn_gap) / 4;
    int btn_h = (ch - (btn_start_y - cy) - 16 - 4 * btn_gap) / 5;
    if (btn_h < 32) btn_h = 32;

    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = disp_x + c * (btn_w + btn_gap);
            int by = btn_start_y + r * (btn_h + btn_gap);
            const char* label = calc_buttons[r][c];

            int is_btn_hover = (calc_mouse_rx >= (bx - cx) && calc_mouse_rx < (bx - cx) + btn_w &&
                                calc_mouse_ry >= (by - cy) && calc_mouse_ry < (by - cy) + btn_h);

            uint32_t btn_bg = is_btn_hover ? 0xFF363A4F : COLOR_OVERLAY;
            uint32_t btn_fg = COLOR_TEXT_PRIMARY;

            // Highlight special buttons
            if (label[0] == '=') {
                btn_bg = is_btn_hover ? 0xFFF9E2AF : COLOR_LEMON_AMBER;
                btn_fg = 0xFF11111B;
            } else if (label[0] == 'C' || label[0] == 'D') {
                btn_bg = is_btn_hover ? 0xFF585B70 : 0xFF45475A;
                btn_fg = COLOR_RED_ACCENT;
            } else if (c == 3) {
                btn_bg = is_btn_hover ? 0xFF45475A : 0xFF363A4F;
                btn_fg = COLOR_BLUE_ACCENT;
            }

            gfx_fill_rounded_rect(bx, by, btn_w, btn_h, 6, btn_bg);
            gfx_draw_rounded_rect(bx, by, btn_w, btn_h, 6, is_btn_hover ? COLOR_LEMON_GOLD : COLOR_BORDER);

            int lw = font_get_string_width(label);
            int lx = bx + (btn_w - lw) / 2;
            int ly = by + (btn_h - FONT_HEIGHT) / 2;
            gfx_draw_text(lx, ly, label, btn_fg);
        }
    }
}

static void calc_on_mouse(window_t* win, int rx, int ry, int buttons, int event_type) {
    (void)win;
    (void)buttons;
    if (event_type == WM_EVENT_MOUSE_MOVE) {
        calc_mouse_rx = rx;
        calc_mouse_ry = ry;
        return;
    }
    if (event_type != WM_EVENT_MOUSE_CLICK) return;

    int disp_x = 12;
    int disp_y = 12;
    int disp_h = 56;
    int cw = win->width - 6;
    int ch = win->height - 35;

    int btn_start_y = disp_y + disp_h + 14;
    int btn_area_w = cw - 24;
    int btn_gap = 8;
    int btn_w = (btn_area_w - 3 * btn_gap) / 4;
    int btn_h = (ch - btn_start_y - 16 - 4 * btn_gap) / 5;
    if (btn_h < 32) btn_h = 32;

    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = disp_x + c * (btn_w + btn_gap);
            int by = btn_start_y + r * (btn_h + btn_gap);

            if (rx >= bx && rx < bx + btn_w && ry >= by && ry < by + btn_h) {
                const char* label = calc_buttons[r][c];
                if (label[0] == 'D' && label[1] == 'E') {
                    calc_input_char('\b');
                } else {
                    calc_input_char(label[0]);
                }
                return;
            }
        }
    }
}

static void calc_on_key(window_t* win, char key) {
    (void)win;
    calc_input_char(key);
}

static void calc_on_close(window_t* win) {
    (void)win;
    calc_win = NULL;
}

void gui_calc_open(void) {
    if (calc_win && !calc_win->is_closed) {
        if (calc_win->is_minimized) {
            wm_restore_window(calc_win);
        }
        wm_bring_to_front(calc_win);
        return;
    }

    calc_state.display[0] = '0';
    calc_state.display[1] = '\0';
    calc_state.display_len = 1;
    calc_state.operand_a = 0;
    calc_state.op = 0;
    calc_state.reset_on_next = 0;

    calc_win = wm_create_window("Calculator", ICON_CALCULATOR, 200, 110, 320, 390, calc_render);
    if (calc_win) {
        calc_win->on_mouse = calc_on_mouse;
        calc_win->on_key = calc_on_key;
        calc_win->on_close = calc_on_close;
    }
}
