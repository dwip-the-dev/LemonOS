#include "gui_converter.h"
#include "graphics.h"
#include "font.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    int category; // 0=Temp, 1=Dist, 2=Weight, 3=Volume, 4=Digital
    int sub_type; // 0..5
    char input_val[32];
    int input_len;
    char result_str[64];
} conv_state_t;

static conv_state_t conv_state;
static window_t* conv_win = NULL;

static const char* cat_names[] = {"Temp", "Distance", "Weight", "Volume", "Digital"};

typedef struct {
    const char* label;
    const char* from_u;
    const char* to_u;
} conv_opt_t;

static const conv_opt_t conv_options[5][6] = {
    // Temp
    {
        {"Celsius -> Fahrenheit", "C", "F"},
        {"Fahrenheit -> Celsius", "F", "C"},
        {"Celsius -> Kelvin", "C", "K"},
        {"Kelvin -> Celsius", "K", "C"},
        {"Fahrenheit -> Kelvin", "F", "K"},
        {"Kelvin -> Fahrenheit", "K", "F"}
    },
    // Distance
    {
        {"km -> Miles", "km", "mi"},
        {"Miles -> km", "mi", "km"},
        {"Meters -> Feet", "m", "ft"},
        {"Feet -> Meters", "ft", "m"},
        {"Centimeters -> Inches", "cm", "in"},
        {"Inches -> Centimeters", "in", "cm"}
    },
    // Weight
    {
        {"Kilograms -> Pounds", "kg", "lbs"},
        {"Pounds -> Kilograms", "lbs", "kg"},
        {"Kilograms -> Grams", "kg", "g"},
        {"Grams -> Kilograms", "g", "kg"},
        {"Kilograms -> Ounces", "kg", "oz"},
        {"Ounces -> Kilograms", "oz", "kg"}
    },
    // Volume
    {
        {"Liters -> Gallons", "L", "gal"},
        {"Gallons -> Liters", "gal", "L"},
        {"Liters -> Milliliters", "L", "mL"},
        {"Milliliters -> Liters", "mL", "L"},
        {"Liters -> Cups", "L", "cups"},
        {"Cups -> Liters", "cups", "L"}
    },
    // Digital
    {
        {"Megabytes -> Gigabytes", "MB", "GB"},
        {"Gigabytes -> Megabytes", "GB", "MB"},
        {"Gigabytes -> Terabytes", "GB", "TB"},
        {"Terabytes -> Gigabytes", "TB", "GB"},
        {"Kilobytes -> Megabytes", "KB", "MB"},
        {"Megabytes -> Kilobytes", "MB", "KB"}
    }
};

static double conv_str_to_d(const char* s) {
    double res = 0.0, frac = 1.0;
    int in_frac = 0, sign = 1;
    if (*s == '-') { sign = -1; s++; }
    while (*s) {
        if (*s == '.') in_frac = 1;
        else if (*s >= '0' && *s <= '9') {
            if (in_frac) { frac *= 0.1; res += (*s - '0') * frac; }
            else { res = res * 10.0 + (*s - '0'); }
        }
        s++;
    }
    return res * sign;
}

static void conv_format_res(double v, const char* to_u) {
    char* out = conv_state.result_str;
    if (v < 0) { *out++ = '-'; v = -v; }
    int ip = (int)v;
    double fp = v - ip;

    char temp[16]; int p = 0;
    if (ip == 0) temp[p++] = '0';
    else { while (ip > 0) { temp[p++] = '0' + (ip % 10); ip /= 10; } }
    while (p > 0) *out++ = temp[--p];

    int fi = (int)(fp * 100.0 + 0.5);
    *out++ = '.';
    if (fi < 10) { *out++ = '0'; *out++ = '0' + fi; }
    else { *out++ = '0' + (fi / 10); *out++ = '0' + (fi % 10); }

    *out++ = ' ';
    while (*to_u) *out++ = *to_u++;
    *out = '\0';
}

static void conv_recalc(void) {
    double in = conv_str_to_d(conv_state.input_val);
    double out = 0.0;
    int cat = conv_state.category;
    int sub = conv_state.sub_type;
    const char* to_u = conv_options[cat][sub].to_u;

    if (cat == 0) { // Temp
        if (sub == 0) out = (in * 9.0 / 5.0) + 32.0;
        else if (sub == 1) out = (in - 32.0) * 5.0 / 9.0;
        else if (sub == 2) out = in + 273.15;
        else if (sub == 3) out = in - 273.15;
        else if (sub == 4) out = (in - 32.0) * 5.0 / 9.0 + 273.15;
        else if (sub == 5) out = (in - 273.15) * 9.0 / 5.0 + 32.0;
    } else if (cat == 1) { // Distance
        if (sub == 0) out = in * 0.621371;
        else if (sub == 1) out = in / 0.621371;
        else if (sub == 2) out = in * 3.28084;
        else if (sub == 3) out = in / 3.28084;
        else if (sub == 4) out = in * 0.393701;
        else if (sub == 5) out = in / 0.393701;
    } else if (cat == 2) { // Weight
        if (sub == 0) out = in * 2.20462;
        else if (sub == 1) out = in / 2.20462;
        else if (sub == 2) out = in * 1000.0;
        else if (sub == 3) out = in / 1000.0;
        else if (sub == 4) out = in * 35.274;
        else if (sub == 5) out = in / 35.274;
    } else if (cat == 3) { // Volume
        if (sub == 0) out = in * 0.264172;
        else if (sub == 1) out = in / 0.264172;
        else if (sub == 2) out = in * 1000.0;
        else if (sub == 3) out = in / 1000.0;
        else if (sub == 4) out = in * 4.22675;
        else if (sub == 5) out = in / 4.22675;
    } else if (cat == 4) { // Digital
        if (sub == 0) out = in / 1024.0;
        else if (sub == 1) out = in * 1024.0;
        else if (sub == 2) out = in / 1024.0;
        else if (sub == 3) out = in * 1024.0;
        else if (sub == 4) out = in / 1024.0;
        else if (sub == 5) out = in * 1024.0;
    }
    conv_format_res(out, to_u);
}

static void conv_render(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win;
    gfx_fill_rect(cx, cy, cw, ch, COLOR_SURFACE);

    // 1. Top Category Tabs
    int tab_w = (cw - 24) / 5;
    int tab_h = 28;
    for (int i = 0; i < 5; i++) {
        int tx = cx + 12 + i * tab_w;
        int ty = cy + 10;
        int is_sel = (conv_state.category == i);

        uint32_t tbg = is_sel ? COLOR_BLUE_ACCENT : COLOR_OVERLAY;
        uint32_t tfg = is_sel ? 0xFF11111B : COLOR_TEXT_PRIMARY;

        gfx_fill_rounded_rect(tx, ty, tab_w - 4, tab_h, 6, tbg);
        int lw = font_get_string_width(cat_names[i]);
        gfx_draw_text(tx + (tab_w - 4 - lw) / 2, ty + 6, cat_names[i], tfg);
    }

    // 2. Conversion Options (Left Column, 6 items)
    int opt_x = cx + 12;
    int opt_y = cy + 48;
    int opt_w = cw / 2 - 10;
    int opt_h = 26;

    for (int i = 0; i < 6; i++) {
        int oy = opt_y + i * (opt_h + 6);
        int is_cur = (conv_state.sub_type == i);

        uint32_t obg = is_cur ? 0xFF363A4F : COLOR_OVERLAY;
        uint32_t ofg = is_cur ? COLOR_LEMON_GOLD : COLOR_TEXT_SECONDARY;

        gfx_fill_rounded_rect(opt_x, oy, opt_w, opt_h, 6, obg);
        if (is_cur) {
            gfx_draw_rounded_rect(opt_x, oy, opt_w, opt_h, 6, COLOR_LEMON_GOLD);
        }
        gfx_draw_text_ellipsis(opt_x + 10, oy + 5, opt_w - 20, conv_options[conv_state.category][i].label, ofg);
    }

    // 3. Right Pane: Input Box & Result Card
    int right_x = cx + cw / 2 + 10;
    int right_w = cw - (right_x - cx) - 12;

    gfx_draw_text(right_x, opt_y, "Enter Value:", COLOR_TEXT_MUTED);

    // Input Card
    int in_y = opt_y + 18;
    int in_h = 36;
    gfx_fill_rounded_rect(right_x, in_y, right_w, in_h, 8, 0xFF181825);
    gfx_draw_rounded_rect(right_x, in_y, right_w, in_h, 8, COLOR_BORDER_LIGHT);

    char in_display[40];
    int ip = 0;
    int jp = 0;
    while (conv_state.input_val[jp]) in_display[ip++] = conv_state.input_val[jp++];
    in_display[ip++] = ' ';
    const char* fu = conv_options[conv_state.category][conv_state.sub_type].from_u;
    while (*fu) in_display[ip++] = *fu++;
    in_display[ip] = '\0';
    gfx_draw_text_clipped(right_x + 10, in_y + 10, right_w - 20, in_display, COLOR_GREEN_ACCENT);

    // Result Card
    int res_y = in_y + in_h + 16;
    int res_h = 60;
    gfx_fill_rounded_rect(right_x, res_y, right_w, res_h, 8, 0xFF181825);
    gfx_draw_rounded_rect(right_x, res_y, right_w, res_h, 8, COLOR_LEMON_GOLD);

    gfx_draw_text(right_x + 10, res_y + 8, "Converted Result:", COLOR_TEXT_MUTED);
    gfx_draw_text_clipped(right_x + 10, res_y + 28, right_w - 20, conv_state.result_str, COLOR_LEMON_GOLD);

    // 4. Quick Keypad Buttons (0-9, ., DEL, C)
    int pad_y = res_y + res_h + 16;
    static const char* pad_keys[3][4] = {
        {"7", "8", "9", "C"},
        {"4", "5", "6", "DEL"},
        {"1", "2", "3", "0"}
    };
    int kw = (right_w - 3 * 6) / 4;
    int kh = 26;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            int kx = right_x + c * (kw + 6);
            int ky = pad_y + r * (kh + 6);
            gfx_fill_rounded_rect(kx, ky, kw, kh, 4, COLOR_OVERLAY);
            gfx_draw_rounded_rect(kx, ky, kw, kh, 4, COLOR_BORDER);
            const char* klabel = pad_keys[r][c];
            int klw = font_get_string_width(klabel);
            gfx_draw_text(kx + (kw - klw) / 2, ky + 5, klabel, COLOR_TEXT_PRIMARY);
        }
    }
}

static void conv_handle_input_char(char c) {
    if (c == 'C' || c == 'c') {
        conv_state.input_val[0] = '0';
        conv_state.input_val[1] = '\0';
        conv_state.input_len = 1;
        conv_recalc();
        return;
    }
    if (c == '\b') {
        if (conv_state.input_len > 1) {
            conv_state.input_len--;
            conv_state.input_val[conv_state.input_len] = '\0';
        } else {
            conv_state.input_val[0] = '0';
            conv_state.input_val[1] = '\0';
            conv_state.input_len = 1;
        }
        conv_recalc();
        return;
    }
    if ((c >= '0' && c <= '9') || c == '.' || (c == '-' && conv_state.input_len == 0)) {
        if (conv_state.input_len == 1 && conv_state.input_val[0] == '0' && c != '.') {
            conv_state.input_val[0] = c;
            conv_state.input_len = 1;
        } else if (conv_state.input_len < 16) {
            if (c == '.') {
                for (int i = 0; i < conv_state.input_len; i++) {
                    if (conv_state.input_val[i] == '.') return;
                }
            }
            conv_state.input_val[conv_state.input_len++] = c;
            conv_state.input_val[conv_state.input_len] = '\0';
        }
        conv_recalc();
    }
}

static void conv_on_mouse(window_t* win, int rx, int ry, int buttons, int event_type) {
    (void)win; (void)buttons;
    if (event_type != WM_EVENT_MOUSE_CLICK) return;
    int cw = win->width - 6;

    // Check category tabs
    int tab_w = (cw - 24) / 5;
    if (ry >= 10 && ry <= 38) {
        for (int i = 0; i < 5; i++) {
            int tx = 12 + i * tab_w;
            if (rx >= tx && rx < tx + tab_w - 4) {
                conv_state.category = i;
                conv_state.sub_type = 0;
                conv_recalc();
                return;
            }
        }
    }

    // Check options list
    int opt_x = 12;
    int opt_y = 48;
    int opt_w = cw / 2 - 10;
    int opt_h = 26;
    for (int i = 0; i < 6; i++) {
        int oy = opt_y + i * (opt_h + 6);
        if (rx >= opt_x && rx < opt_x + opt_w && ry >= oy && ry < oy + opt_h) {
            conv_state.sub_type = i;
            conv_recalc();
            return;
        }
    }

    // Check keypad
    int right_x = cw / 2 + 10;
    int right_w = cw - right_x - 12;
    int pad_y = opt_y + 18 + 36 + 16 + 60 + 16;
    int kw = (right_w - 3 * 6) / 4;
    int kh = 26;

    static const char* pad_keys[3][4] = {
        {"7", "8", "9", "C"},
        {"4", "5", "6", "DEL"},
        {"1", "2", "3", "0"}
    };

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            int kx = right_x + c * (kw + 6);
            int ky = pad_y + r * (kh + 6);
            if (rx >= kx && rx < kx + kw && ry >= ky && ry < ky + kh) {
                const char* klabel = pad_keys[r][c];
                if (klabel[0] == 'D') conv_handle_input_char('\b');
                else conv_handle_input_char(klabel[0]);
                return;
            }
        }
    }
}

static void conv_on_key(window_t* win, char key) {
    (void)win;
    conv_handle_input_char(key);
}

static void conv_on_close(window_t* win) {
    (void)win;
    conv_win = NULL;
}

void gui_converter_open(void) {
    if (conv_win && !conv_win->is_closed) {
        if (conv_win->is_minimized) wm_restore_window(conv_win);
        wm_bring_to_front(conv_win);
        return;
    }

    conv_state.category = 0;
    conv_state.sub_type = 0;
    conv_state.input_val[0] = '1';
    conv_state.input_val[1] = '0';
    conv_state.input_val[2] = '0';
    conv_state.input_val[3] = '\0';
    conv_state.input_len = 3;
    conv_recalc();

    conv_win = wm_create_window("Unit Converter", ICON_CONVERT, 160, 100, 560, 390, conv_render);
    if (conv_win) {
        conv_win->on_mouse = conv_on_mouse;
        conv_win->on_key = conv_on_key;
        conv_win->on_close = conv_on_close;
    }
}
