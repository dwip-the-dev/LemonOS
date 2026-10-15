#include "desktop.h"
#include "graphics.h"
#include "font.h"
#include "icons.h"
#include "mouse.h"
#include "keyboard.h"
#include "wm.h"
#include "bga.h"
#include "rtc.h"
#include "shutdown.h"
#include "reboot.h"
#include "alerts.h"
#include "net.h"
#include "string.h"
#include "filesystem.h"
#include "auth.h"
#include "gui_engine.h"
#include "posix_time.h"
#include "fast_mem.h"

// All ported GUI apps
#include "gui_firefox.h"
#include "gui_terminal.h"
#include "gui_calc.h"
#include "gui_converter.h"
#include "gui_sysinfo.h"
#include "gui_files.h"
#include "gui_clock.h"
#include "gui_snake.h"
#include "gui_pong.h"
#include "gui_breakout.h"
#include "gui_matrix.h"
#include "gui_settings.h"
#include "gui_services.h"
#include "gui_speedtest.h"

static int desktop_running = 0;
static int current_theme = THEME_LEMON_GOLD;

// Popups and overlays state
static int start_menu_open = 0;
static int power_menu_open = 0;
static int calendar_open = 0;
static int network_popup_open = 0;
static int volume_popup_open = 0;
static int notifications_open = 0;
static int show_desktop_state = 0;

static int audio_volume = 80; // 0..100
static int audio_muted = 0;

static const char* day_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char* month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// Day of week calculation via Zeller's congruence
static int calculate_dow(int d, int m, int y) {
    int yr = (y < 100) ? (y + 2000) : y;
    if (m < 3) {
        m += 12;
        yr -= 1;
    }
    int k = yr % 100;
    int j = yr / 100;
    int h = (d + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    int dow = (h + 6) % 7; // 0 = Sun, 1 = Mon ...
    return (dow >= 0 && dow <= 6) ? dow : 0;
}

// Desktop Icons on the left
typedef struct {
    const char* label;
    icon_type_t icon_id;
    int x, y;
    void (*launch)(void);
} desktop_icon_t;

void app_doom_launch(void);

static desktop_icon_t desktop_icons[] = {
    // Column 1 (x = 24)
    {"Browser",     ICON_FIREFOX,      24,  50,  gui_firefox_open},
    {"Terminal",    ICON_TERMINAL,     24, 140,  gui_terminal_open},
    {"Files",       ICON_FILE_MANAGER, 24, 230,  gui_files_open},
    {"Calculator",  ICON_CALCULATOR,   24, 320,  gui_calc_open},
    {"Converter",   ICON_CONVERTER,    24, 410,  gui_converter_open},
    {"Clock",       ICON_CLOCK,        24, 500,  gui_clock_open},
    // Column 2 (x = 124)
    {"System Info", ICON_SYSINFO,     124,  50,  gui_sysinfo_open},
    {"Services",    ICON_SETTINGS,    124, 140,  gui_services_open},
    {"Snake",       ICON_GAMES,       124, 230,  gui_snake_open},
    {"Breakout",    ICON_GAMES,       124, 320,  gui_breakout_open},
    {"Pong",        ICON_GAMES,       124, 410,  gui_pong_open},
    {"Matrix",      ICON_MATRIX,      124, 500,  gui_matrix_open},
    {"DOOM",        ICON_GAMES,       124, 590,  app_doom_launch}
};
#define NUM_DESKTOP_ICONS (sizeof(desktop_icons) / sizeof(desktop_icons[0]))

// Desktop icon dragging state
static int dragging_icon_idx = -1;
static int icon_drag_start_x = 0;
static int icon_drag_start_y = 0;
static int icon_drag_off_x = 0;
static int icon_drag_off_y = 0;
static int icon_has_moved = 0;

// Desktop rubberband selection marquee
static int marquee_active = 0;
static int marquee_start_x = 0;
static int marquee_start_y = 0;
static int marquee_cur_x = 0;
static int marquee_cur_y = 0;

// Pinned App Launchers in Taskbar
typedef struct {
    icon_type_t icon_id;
    const char* name;
    void (*launch)(void);
} pinned_item_t;

static const pinned_item_t pinned_items[] = {
    {ICON_FIREFOX,      "Browser",    gui_firefox_open},
    {ICON_TERMINAL,     "Terminal",   gui_terminal_open},
    {ICON_FILE_MANAGER, "Files",      gui_files_open},
    {ICON_CALCULATOR,   "Calc",       gui_calc_open},
    {ICON_CLOCK,        "Clock",      gui_clock_open},
    {ICON_SYSINFO,      "About",      gui_sysinfo_open},
    {ICON_SETTINGS,     "Settings",   gui_settings_open}
};
#define NUM_PINNED_ITEMS (sizeof(pinned_items) / sizeof(pinned_items[0]))

// App launchers implementation for external access
void app_firefox_launch(void)        { gui_firefox_open(); }
void app_terminal_launch(void)       { gui_terminal_open(); }
void app_calculator_launch(void)     { gui_calc_open(); }
void app_unit_converter_launch(void) { gui_converter_open(); }
void app_system_info_launch(void)    { gui_sysinfo_open(); }
void app_file_manager_launch(void)   { gui_files_open(); }
void app_clock_launch(void)          { gui_clock_open(); }
void app_snake_launch(void)          { gui_snake_open(); }
void app_pong_launch(void)           { gui_pong_open(); }
void app_breakout_launch(void)       { gui_breakout_open(); }
void app_matrix_launch(void)         { gui_matrix_open(); }
void app_settings_launch(void)       { gui_settings_open(); }
void app_doom_launch(void)           {
    extern void doom_main(int argc, char** argv);
    char* doom_argv[] = { "doom", "-warp", "1", "1", NULL };
    doom_main(4, doom_argv);
    if (bga_is_available()) {
        bga_set_video_mode(SCREEN_WIDTH, SCREEN_HEIGHT, 32, 1, 1);
    }
    mouse_init(SCREEN_WIDTH, SCREEN_HEIGHT);
    keyboard_clear_events();
}

static int has_custom_wallpaper = 0;
static char current_wallpaper_path[128] = "";
static uint32_t custom_wallpaper_cache[SCREEN_WIDTH * SCREEN_HEIGHT];

static void generate_matrix_wallpaper(void) {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint32_t bg = 0xFF050805;
            int col = x / 16;
            int phase = (y + col * 47) % 180;
            if (phase < 60) {
                uint8_t g = (60 - phase) * 4;
                if (phase == 0) bg = 0xFFE0FFE0;
                else bg = COLOR_RGB(10, g, 20);
            }
            custom_wallpaper_cache[y * SCREEN_WIDTH + x] = bg;
        }
    }
}

static void generate_vaporwave_wallpaper(void) {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            if (y < SCREEN_HEIGHT / 2) {
                uint8_t r = 30 + (y * 180) / (SCREEN_HEIGHT / 2);
                uint8_t g = 10 + (y * 30) / (SCREEN_HEIGHT / 2);
                uint8_t b = 60 + (y * 120) / (SCREEN_HEIGHT / 2);
                uint32_t c = COLOR_RGB(r, g, b);

                int dx = x - SCREEN_WIDTH / 2;
                int dy = y - (SCREEN_HEIGHT / 2 - 20);
                if (dx * dx + dy * dy < 120 * 120) {
                    if ((y % 16) < 11) {
                        c = 0xFFF9E2AF;
                    }
                }
                custom_wallpaper_cache[y * SCREEN_WIDTH + x] = c;
            } else {
                int gy = y - SCREEN_HEIGHT / 2;
                uint32_t c = 0xFF0F0B1A;
                if ((gy % 20) == 0) c = 0xFFF38BA8;
                int gx = x - SCREEN_WIDTH / 2;
                if (gy > 0) {
                    int p_x = (gx * 100) / gy;
                    if (p_x % 40 >= -1 && p_x % 40 <= 1) c = 0xFF89B4FA;
                }
                custom_wallpaper_cache[y * SCREEN_WIDTH + x] = c;
            }
        }
    }
}

static void generate_aurora_wallpaper(void) {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint8_t r = 10 + (y * 20) / SCREEN_HEIGHT;
            uint8_t g = 15 + (y * 40) / SCREEN_HEIGHT;
            uint8_t b = 35 + (y * 60) / SCREEN_HEIGHT;
            int wave = (int)(80 * ((x % 300) < 150 ? (x % 300) : (300 - (x % 300)))) / 150;
            if (y > 140 + wave && y < 280 + wave) {
                g += 80;
                b += 60;
                r += 40;
            }
            int mtn = SCREEN_HEIGHT - 120 - (((x * 7) % 250) < 125 ? ((x * 7) % 250) : (250 - ((x * 7) % 250)));
            if (y > mtn) {
                r = 6; g = 8; b = 14;
            }
            custom_wallpaper_cache[y * SCREEN_WIDTH + x] = COLOR_RGB(r, g, b);
        }
    }
}

static void generate_cyberpunk_wallpaper(void) {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint32_t c = gfx_blend(0xFF0F0F23, 0xFF050510, (y * 255) / SCREEN_HEIGHT);
            int dx = x - (SCREEN_WIDTH - 220);
            int dy = y - 140;
            if (dx * dx + dy * dy < 50 * 50) {
                int cdx = dx + 16;
                if (cdx * cdx + dy * dy >= 46 * 46) c = COLOR_LEMON_GOLD;
            }
            int b_idx = x / 48;
            int b_h = 150 + ((b_idx * 97) % 300);
            if (y >= SCREEN_HEIGHT - b_h) {
                c = 0xFF141424;
                int wx = x % 48;
                int wy = (SCREEN_HEIGHT - y) % 24;
                if (wx >= 10 && wx <= 18 && wy >= 6 && wy <= 16) {
                    if (((b_idx * 13 + y / 24) % 3) == 0) c = COLOR_LEMON_GOLD;
                    else if (((b_idx * 7 + y / 24) % 4) == 0) c = 0xFF89DCEB;
                }
            }
            custom_wallpaper_cache[y * SCREEN_WIDTH + x] = c;
        }
    }
}

static int load_bmp_file(const char* filepath) {
    char* data = fs_read_file(filepath);
    if (!data) return -1;

    size_t size = fs_get_file_size(filepath);
    if (size < 54) return -1;

    if (data[0] != 'B' || data[1] != 'M') return -1;

    uint32_t data_offset = *(uint32_t*)(data + 10);
    int32_t img_w = *(int32_t*)(data + 18);
    int32_t img_h = *(int32_t*)(data + 22);
    uint16_t bpp = *(uint16_t*)(data + 28);
    uint32_t compression = *(uint32_t*)(data + 30);

    if (bpp != 24 && bpp != 32) return -1;
    if (compression != 0) return -1;

    int abs_h = (img_h < 0) ? -img_h : img_h;
    if (img_w <= 0 || abs_h <= 0) return -1;

    int row_stride = ((bpp * img_w + 31) / 32) * 4;
    const uint8_t* pixels = (const uint8_t*)(data + data_offset);

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        int src_y = (y * abs_h) / SCREEN_HEIGHT;
        if (img_h > 0) {
            src_y = abs_h - 1 - src_y;
        }
        const uint8_t* row = pixels + src_y * row_stride;
        int bytes_per_pixel = bpp / 8;

        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int src_x = (x * img_w) / SCREEN_WIDTH;
            const uint8_t* px = row + src_x * bytes_per_pixel;
            uint8_t b = px[0];
            uint8_t g = px[1];
            uint8_t r = px[2];
            custom_wallpaper_cache[y * SCREEN_WIDTH + x] = COLOR_RGB(r, g, b);
        }
    }

    has_custom_wallpaper = 1;
    strncpy(current_wallpaper_path, filepath, sizeof(current_wallpaper_path) - 1);
    return 0;
}

int desktop_set_wallpaper(const char* filepath) {
    if (!filepath || !filepath[0] || strcmp(filepath, "default") == 0) {
        desktop_reset_wallpaper();
        alert_send("Wallpaper Reset", "Theme Gradient Applied", ICON_SETTINGS, ALERT_TYPE_INFO);
        return 0;
    }

    if (strcmp(filepath, "matrix") == 0 || strstr(filepath, "matrix")) {
        generate_matrix_wallpaper();
        has_custom_wallpaper = 1;
        strncpy(current_wallpaper_path, "Matrix Cyberpunk (Green Rain)", sizeof(current_wallpaper_path) - 1);
        alert_send("Wallpaper Updated", "Matrix Cyberpunk Active", ICON_SETTINGS, ALERT_TYPE_SUCCESS);
        return 0;
    } else if (strcmp(filepath, "vaporwave") == 0 || strstr(filepath, "vaporwave")) {
        generate_vaporwave_wallpaper();
        has_custom_wallpaper = 1;
        strncpy(current_wallpaper_path, "Retro Vaporwave (Synth Sunset)", sizeof(current_wallpaper_path) - 1);
        alert_send("Wallpaper Updated", "Retro Vaporwave Active", ICON_SETTINGS, ALERT_TYPE_SUCCESS);
        return 0;
    } else if (strcmp(filepath, "aurora") == 0 || strstr(filepath, "aurora")) {
        generate_aurora_wallpaper();
        has_custom_wallpaper = 1;
        strncpy(current_wallpaper_path, "Neon Aurora (Northern Lights)", sizeof(current_wallpaper_path) - 1);
        alert_send("Wallpaper Updated", "Neon Aurora Active", ICON_SETTINGS, ALERT_TYPE_SUCCESS);
        return 0;
    } else if (strcmp(filepath, "cyberpunk") == 0 || strstr(filepath, "cyberpunk")) {
        generate_cyberpunk_wallpaper();
        has_custom_wallpaper = 1;
        strncpy(current_wallpaper_path, "Cyberpunk City (Neon Skyline)", sizeof(current_wallpaper_path) - 1);
        alert_send("Wallpaper Updated", "Cyberpunk City Active", ICON_SETTINGS, ALERT_TYPE_SUCCESS);
        return 0;
    }

    if (load_bmp_file(filepath) == 0) {
        alert_send("Wallpaper Updated", filepath, ICON_SETTINGS, ALERT_TYPE_SUCCESS);
        return 0;
    }

    return -1;
}

const char* desktop_get_wallpaper(void) {
    if (has_custom_wallpaper) return current_wallpaper_path;
    return "Default Theme Gradient";
}

void desktop_reset_wallpaper(void) {
    has_custom_wallpaper = 0;
    current_wallpaper_path[0] = '\0';
}

int desktop_has_custom_wallpaper(void) {
    return has_custom_wallpaper;
}

void desktop_set_theme(int theme_id) {
    if (theme_id >= 0 && theme_id <= 3) {
        current_theme = theme_id;
        has_custom_wallpaper = 0;
    }
}

int desktop_get_theme(void) {
    return current_theme;
}

void desktop_exit(void) {
    desktop_running = 0;
    bga_disable();
}

static uint32_t wallpaper_cache[SCREEN_WIDTH * SCREEN_HEIGHT];
static int wallpaper_cached = 0;

static void wp_fill_circle(int cx, int cy, int r, uint32_t color) {
    int r2 = r * r;
    for (int dy = -r; dy <= r; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= SCREEN_HEIGHT) continue;
        int row = py * SCREEN_WIDTH;
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r2) {
                int px = cx + dx;
                if (px >= 0 && px < SCREEN_WIDTH) {
                    wallpaper_cache[row + px] = color;
                }
            }
        }
    }
}

static void wp_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    int r2 = r * r;
    for (int py = y; py < y + h; py++) {
        if (py < 0 || py >= SCREEN_HEIGHT) continue;
        int row = py * SCREEN_WIDTH;
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= SCREEN_WIDTH) continue;
            int in_corner = 0;
            int dx = 0, dy = 0;
            if (px < x + r && py < y + r) { dx = (x + r) - px; dy = (y + r) - py; in_corner = 1; }
            else if (px >= x + w - r && py < y + r) { dx = px - (x + w - r - 1); dy = (y + r) - py; in_corner = 1; }
            else if (px < x + r && py >= y + h - r) { dx = (x + r) - px; dy = py - (y + h - r - 1); in_corner = 1; }
            else if (px >= x + w - r && py >= y + h - r) { dx = px - (x + w - r - 1); dy = py - (y + h - r - 1); in_corner = 1; }
            if (in_corner && (dx * dx + dy * dy > r2)) continue;
            wallpaper_cache[row + px] = color;
        }
    }
}

static void wp_draw_ring(int cx, int cy, int r, int thick, uint32_t color) {
    int r_out = r * r;
    int r_in = (r - thick) * (r - thick);
    for (int dy = -r; dy <= r; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= SCREEN_HEIGHT) continue;
        int row = py * SCREEN_WIDTH;
        for (int dx = -r; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 <= r_out && d2 >= r_in) {
                int px = cx + dx;
                if (px >= 0 && px < SCREEN_WIDTH) {
                    wallpaper_cache[row + px] = color;
                }
            }
        }
    }
}

static void render_wallpaper_to_cache(void) {
    // 1. Flat, calm, minimalist background (matte deep obsidian slate)
    fast_memset32(wallpaper_cache, 0xFF141620, SCREEN_WIDTH * SCREEN_HEIGHT);

    // 2. Abstract 2D Geometric Shapes (Nordic / Bauhaus minimalist art)
    // Shape 1: Serene deep slate disc (anchors the right/center)
    wp_fill_circle(620, 360, 200, 0xFF222838);

    // Shape 2: Overlapping warm terracotta / clay arched capsule
    wp_fill_rounded_rect(410, 210, 170, 320, 85, 0xFF8A584E);

    // Shape 3: Calm eucalyptus / sage green curved semi-circle
    wp_fill_circle(520, 450, 130, 0xFF46635F);

    // Shape 4: Minimalist floating pill bar in warm amber / sand
    wp_fill_rounded_rect(330, 380, 140, 34, 17, 0xFFD4A373);

    // Shape 5: Delicate geometric slate outline ring
    wp_draw_ring(710, 240, 68, 3, 0xFF354154);

    // Shape 6: Minimalist terracotta accent dot
    wp_fill_circle(350, 240, 22, 0xFFDE7456);

    // Shape 7: Muted soft sage teal accent disk
    wp_fill_circle(760, 500, 40, 0xFF547472);

    // Shape 8: Clean fine horizontal accent rule
    for (int px = 590; px < 730; px++) {
        wallpaper_cache[480 * SCREEN_WIDTH + px] = 0xFFD4A373;
        wallpaper_cache[481 * SCREEN_WIDTH + px] = 0xFFD4A373;
    }
}

static void draw_wallpaper(void) {
    if (has_custom_wallpaper) {
        fast_memcpy(gfx_get_backbuffer(), custom_wallpaper_cache, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
        return;
    }

    if (!wallpaper_cached) {
        render_wallpaper_to_cache();
        wallpaper_cached = 1;
    }

    // Blazing-fast background restore (<0.5ms via x86 REP MOVSD)
    fast_memcpy(gfx_get_backbuffer(), wallpaper_cache, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
}

static void draw_desktop_icons(int mouse_x, int mouse_y) {
    int m_x1 = 0, m_y1 = 0, m_w = 0, m_h = 0;
    if (marquee_active) {
        m_x1 = (marquee_start_x < marquee_cur_x) ? marquee_start_x : marquee_cur_x;
        m_y1 = (marquee_start_y < marquee_cur_y) ? marquee_start_y : marquee_cur_y;
        m_w = (marquee_start_x > marquee_cur_x) ? (marquee_start_x - marquee_cur_x) : (marquee_cur_x - marquee_start_x);
        m_h = (marquee_start_y > marquee_cur_y) ? (marquee_start_y - marquee_cur_y) : (marquee_cur_y - marquee_start_y);
    }

    for (size_t i = 0; i < NUM_DESKTOP_ICONS; i++) {
        int ix = desktop_icons[i].x;
        int iy = desktop_icons[i].y;
        gui_rect_t box;
        gui_get_desktop_icon_box(ix, iy, &box);
        int is_hover = gui_hitbox_contains(box.x, box.y, box.w, box.h, mouse_x, mouse_y);
        if ((int)i == dragging_icon_idx) {
            is_hover = 1;
        } else if (marquee_active && m_w > 2 && m_h > 2) {
            if (box.x < m_x1 + m_w && box.x + box.w > m_x1 &&
                box.y < m_y1 + m_h && box.y + box.h > m_y1) {
                is_hover = 1;
            }
        }
        gui_render_desktop_tile(ix, iy, desktop_icons[i].label, (int)desktop_icons[i].icon_id, is_hover);
    }
}

static void draw_top_bar(int mouse_x, int mouse_y) {
    // 1. Top Bar Acrylic Surface with 1px bottom border
    gfx_fill_rect(0, 0, SCREEN_WIDTH, 30, 0xFF11111B);
    gfx_fill_rect(0, 29, SCREEN_WIDTH, 1, COLOR_OVERLAY);
    for (int px = 0; px < SCREEN_WIDTH; px++) {
        gfx_put_pixel_alpha(px, 0, 0xFFFFFF, 35);
    }

    // 2. Left: "Activities" Button (Toggles Mission Control Overview)
    int act_hover = (mouse_x >= 6 && mouse_x < 118 && mouse_y >= 3 && mouse_y < 27);
    if (wm_overview_is_open() || act_hover) {
        gfx_fill_rounded_rect(6, 3, 112, 24, 6, COLOR_OVERLAY);
        gfx_draw_rounded_rect(6, 3, 112, 24, 6, COLOR_BORDER_LIGHT);
    }
    icon_draw_16(12, 7, ICON_RESTORE, COLOR_LEMON_GOLD);
    gfx_draw_text(34, 7, "Activities", wm_overview_is_open() ? COLOR_LEMON_GOLD : COLOR_TEXT_PRIMARY);

    // Active Application Name badge with strict clipping and ellipsis
    window_t* active_win = wm_get_active_window();
    if (active_win && !active_win->is_closed) {
        gfx_draw_text(124, 7, "|", COLOR_BORDER_LIGHT);
        int title_x = 136;
        if (active_win->icon_id) {
            icon_draw_16(136, 7, (icon_type_t)active_win->icon_id, COLOR_TEXT_PRIMARY);
            title_x = 158;
        }
        int max_title_w = (SCREEN_WIDTH / 2 - 130) - title_x;
        if (max_title_w > 20) {
            gfx_draw_text_ellipsis(title_x, 7, max_title_w, active_win->title, COLOR_TEXT_SECONDARY);
        }
    }

    // 3. Center: Live Date & Time pill
    int h, m, s, day, month, year;
    get_rtc_time(&h, &m, &s);
    get_rtc_date(&day, &month, &year);

    char clock_str[48];
    int cp = 0;
    int dow = calculate_dow(day, month, year);
    const char* dn = day_names[dow];
    while (*dn) clock_str[cp++] = *dn++;
    clock_str[cp++] = ' ';
    if (month >= 1 && month <= 12) {
        const char* mn = month_names[month - 1];
        while (*mn) clock_str[cp++] = *mn++;
    }
    clock_str[cp++] = ' ';
    clock_str[cp++] = '0' + (day / 10);
    clock_str[cp++] = '0' + (day % 10);
    clock_str[cp++] = ' ';
    clock_str[cp++] = ' ';
    clock_str[cp++] = '0' + (h / 10);
    clock_str[cp++] = '0' + (h % 10);
    clock_str[cp++] = ':';
    clock_str[cp++] = '0' + (m / 10);
    clock_str[cp++] = '0' + (m % 10);
    clock_str[cp++] = ':';
    clock_str[cp++] = '0' + (s / 10);
    clock_str[cp++] = '0' + (s % 10);
    clock_str[cp] = '\0';

    int cw_w = font_get_string_width(clock_str);
    int cw_x = (SCREEN_WIDTH - cw_w) / 2;
    int clock_hover = (mouse_x >= cw_x - 12 && mouse_x < cw_x + cw_w + 12 && mouse_y >= 3 && mouse_y < 27);
    if (calendar_open || clock_hover) {
        gfx_fill_rounded_rect(cw_x - 12, 3, cw_w + 24, 24, 6, COLOR_OVERLAY);
    }
    gfx_draw_text(cw_x, 7, clock_str, calendar_open ? COLOR_LEMON_GOLD : COLOR_TEXT_PRIMARY);

    // 4. Right: System Tray & Status Controls
    // CPU & RAM Gauge
    int perf_x = SCREEN_WIDTH - 290;
    gfx_fill_rounded_rect(perf_x, 4, 110, 22, 6, 0xFF181825);
    gfx_draw_text(perf_x + 6, 7, "CPU 8%", COLOR_GREEN_ACCENT);
    gfx_draw_text(perf_x + 52, 7, "| RAM 18M", COLOR_TEXT_MUTED);

    // Audio Volume Pill
    int vol_x = SCREEN_WIDTH - 170;
    int vol_hover = (mouse_x >= vol_x && mouse_x < vol_x + 28 && mouse_y >= 3 && mouse_y < 27);
    if (volume_popup_open || vol_hover) gfx_fill_rounded_rect(vol_x, 3, 28, 24, 6, COLOR_OVERLAY);
    icon_draw_16(vol_x + 6, 7, ICON_SPEAKER, audio_muted ? COLOR_RED_ACCENT : COLOR_TEXT_PRIMARY);

    // Network Pill
    int net_x = SCREEN_WIDTH - 138;
    int net_hover = (mouse_x >= net_x && mouse_x < net_x + 28 && mouse_y >= 3 && mouse_y < 27);
    if (network_popup_open || net_hover) gfx_fill_rounded_rect(net_x, 3, 28, 24, 6, COLOR_OVERLAY);
    icon_draw_16(net_x + 6, 7, ICON_WIFI, COLOR_GREEN_ACCENT);

    // Notification Center Bell Pill
    int notif_x = SCREEN_WIDTH - 106;
    int notif_hover = (mouse_x >= notif_x && mouse_x < notif_x + 36 && mouse_y >= 3 && mouse_y < 27);
    if (notifications_open || notif_hover) gfx_fill_rounded_rect(notif_x, 3, 36, 24, 6, COLOR_OVERLAY);
    icon_draw_16(notif_x + 4, 7, ICON_SYSINFO, COLOR_TEXT_PRIMARY);

    int unread = alert_get_unread_count();
    if (unread > 0) {
        gfx_fill_rounded_rect(notif_x + 20, 6, 12, 12, 6, COLOR_RED_ACCENT);
        char unread_str[4];
        unread_str[0] = (unread > 9) ? '+' : ('0' + unread);
        unread_str[1] = '\0';
        gfx_draw_text(notif_x + 23, 6, unread_str, 0xFFFFFFFF);
    }

    // Power / Action Menu Pill
    int pwr_x = SCREEN_WIDTH - 66;
    int pwr_hover = (mouse_x >= pwr_x && mouse_x < pwr_x + 58 && mouse_y >= 3 && mouse_y < 27);
    if (power_menu_open || pwr_hover) gfx_fill_rounded_rect(pwr_x, 3, 58, 24, 6, COLOR_OVERLAY);
    icon_draw_16(pwr_x + 8, 7, ICON_BATTERY, COLOR_GREEN_ACCENT);
    icon_draw_16(pwr_x + 34, 7, ICON_POWER, COLOR_RED_ACCENT);
}

// Interactive System Tray Popups & Drawers
static void draw_calendar_popup(void) {
    int cw_w = 280;
    int cw_h = 240;
    int cw_x = (SCREEN_WIDTH - cw_w) / 2;
    int cw_y = 34;

    gfx_draw_shadow(cw_x, cw_y, cw_w, cw_h, 10, 130);
    gfx_fill_rounded_rect(cw_x, cw_y, cw_w, cw_h, 12, 0xFA1E1E2E);
    gfx_draw_rounded_rect(cw_x, cw_y, cw_w, cw_h, 12, COLOR_BORDER_LIGHT);

    int day, month, year;
    get_rtc_date(&day, &month, &year);

    // Month & Year Title
    char my_str[32];
    int p = 0;
    if (month >= 1 && month <= 12) {
        const char* mn = month_names[month - 1];
        while (*mn) my_str[p++] = *mn++;
    }
    my_str[p++] = ' ';
    int y2 = year % 100;
    my_str[p++] = '2'; my_str[p++] = '0';
    my_str[p++] = '0' + (y2 / 10);
    my_str[p++] = '0' + (y2 % 10);
    my_str[p] = '\0';

    gfx_draw_text(cw_x + 16, cw_y + 12, my_str, COLOR_LEMON_GOLD);

    // Days of week row
    const char* dheaders[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    int cell_w = (cw_w - 32) / 7;
    for (int d = 0; d < 7; d++) {
        gfx_draw_text(cw_x + 18 + d * cell_w, cw_y + 36, dheaders[d], COLOR_TEXT_MUTED);
    }
    gfx_fill_rect(cw_x + 16, cw_y + 50, cw_w - 32, 1, COLOR_OVERLAY);

    // Calendar grid
    int first_dow = calculate_dow(1, month, year);
    int total_days = (month >= 1 && month <= 12) ? days_in_month[month - 1] : 30;

    int cur_d = 1;
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 7; col++) {
            if (row == 0 && col < first_dow) continue;
            if (cur_d > total_days) break;

            int dx = cw_x + 18 + col * cell_w;
            int dy = cw_y + 58 + row * 26;

            if (cur_d == day) {
                gfx_fill_rounded_rect(dx - 4, dy - 2, 22, 20, 10, COLOR_LEMON_GOLD);
                char dstr[4];
                dstr[0] = (cur_d >= 10) ? ('0' + cur_d / 10) : ('0' + cur_d);
                dstr[1] = (cur_d >= 10) ? ('0' + cur_d % 10) : '\0';
                dstr[2] = '\0';
                gfx_draw_text(dx, dy, dstr, 0xFF11111B);
            } else {
                char dstr[4];
                dstr[0] = (cur_d >= 10) ? ('0' + cur_d / 10) : ('0' + cur_d);
                dstr[1] = (cur_d >= 10) ? ('0' + cur_d % 10) : '\0';
                dstr[2] = '\0';
                gfx_draw_text(dx, dy, dstr, COLOR_TEXT_SECONDARY);
            }
            cur_d++;
        }
    }

    // Uptime Footer
    gfx_fill_rect(cw_x + 16, cw_y + cw_h - 36, cw_w - 32, 1, COLOR_OVERLAY);
    gfx_draw_text(cw_x + 16, cw_y + cw_h - 24, "LemonOS 3.0 Protected Mode Kernel", COLOR_TEXT_MUTED);
}

static void draw_network_popup(void) {
    int nw_w = 300;
    int nw_h = 240;
    int nw_x = SCREEN_WIDTH - nw_w - 12;
    int nw_y = 34;

    gfx_draw_shadow(nw_x, nw_y, nw_w, nw_h, 10, 130);
    gfx_fill_rounded_rect(nw_x, nw_y, nw_w, nw_h, 12, 0xFA1E1E2E);
    gfx_draw_rounded_rect(nw_x, nw_y, nw_w, nw_h, 12, COLOR_BORDER_LIGHT);

    icon_draw_16(nw_x + 16, nw_y + 12, ICON_WIFI, COLOR_GREEN_ACCENT);
    gfx_draw_text(nw_x + 38, nw_y + 12, "Network Settings (e1000 Gigabit)", COLOR_TEXT_PRIMARY);
    gfx_draw_text(nw_x + 16, nw_y + 32, "Status: Connected (1000 Mbps Full-Duplex)", COLOR_GREEN_ACCENT);

    gfx_fill_rect(nw_x + 16, nw_y + 48, nw_w - 32, 1, COLOR_OVERLAY);

    net_config_t cfg;
    net_get_config(&cfg);

    char ip_str[24], sub_str[24], gw_str[24], dns_str[24], mac_str[24];
    net_ip_to_str(cfg.ip, ip_str);
    net_ip_to_str(cfg.subnet_mask, sub_str);
    net_ip_to_str(cfg.gateway, gw_str);
    net_ip_to_str(cfg.dns_server, dns_str);
    net_mac_to_str(cfg.mac, mac_str);

    gfx_draw_text(nw_x + 16, nw_y + 56,  "IP Address:  ", COLOR_TEXT_MUTED);
    gfx_draw_text(nw_x + 110, nw_y + 56, ip_str, COLOR_TEXT_PRIMARY);

    gfx_draw_text(nw_x + 16, nw_y + 76,  "Subnet Mask: ", COLOR_TEXT_MUTED);
    gfx_draw_text(nw_x + 110, nw_y + 76, sub_str, COLOR_TEXT_PRIMARY);

    gfx_draw_text(nw_x + 16, nw_y + 96,  "Gateway:     ", COLOR_TEXT_MUTED);
    gfx_draw_text(nw_x + 110, nw_y + 96, gw_str, COLOR_TEXT_PRIMARY);

    gfx_draw_text(nw_x + 16, nw_y + 116, "DNS Server:  ", COLOR_TEXT_MUTED);
    gfx_draw_text(nw_x + 110, nw_y + 116, dns_str, COLOR_TEXT_PRIMARY);

    gfx_draw_text(nw_x + 16, nw_y + 136, "MAC Address: ", COLOR_TEXT_MUTED);
    gfx_draw_text(nw_x + 110, nw_y + 136, mac_str, COLOR_TEXT_PRIMARY);

    // Action Buttons: [Ping Gateway] [Renew DHCP]
    int btn_w = (nw_w - 40) / 2;
    int btn_y = nw_y + nw_h - 40;

    gfx_fill_rounded_rect(nw_x + 16, btn_y, btn_w, 28, 6, COLOR_OVERLAY);
    gfx_draw_rounded_rect(nw_x + 16, btn_y, btn_w, 28, 6, COLOR_BORDER_LIGHT);
    gfx_draw_text(nw_x + 24, btn_y + 7, "Ping Gateway", COLOR_GREEN_ACCENT);

    gfx_fill_rounded_rect(nw_x + 24 + btn_w, btn_y, btn_w, 28, 6, COLOR_OVERLAY);
    gfx_draw_rounded_rect(nw_x + 24 + btn_w, btn_y, btn_w, 28, 6, COLOR_BORDER_LIGHT);
    gfx_draw_text(nw_x + 32 + btn_w, btn_y + 7, "Renew DHCP", COLOR_BLUE_ACCENT);
}

static void draw_volume_popup(void) {
    int vw_w = 220;
    int vw_h = 100;
    int vw_x = SCREEN_WIDTH - vw_w - 60;
    int vw_y = 34;

    gfx_draw_shadow(vw_x, vw_y, vw_w, vw_h, 10, 130);
    gfx_fill_rounded_rect(vw_x, vw_y, vw_w, vw_h, 12, 0xFA1E1E2E);
    gfx_draw_rounded_rect(vw_x, vw_y, vw_w, vw_h, 12, COLOR_BORDER_LIGHT);

    icon_draw_16(vw_x + 14, vw_y + 12, ICON_SPEAKER, audio_muted ? COLOR_RED_ACCENT : COLOR_TEXT_PRIMARY);
    gfx_draw_text(vw_x + 36, vw_y + 12, "Sound Blaster 16", COLOR_TEXT_PRIMARY);

    // Volume Slider Bar
    int bar_x = vw_x + 14;
    int bar_y = vw_y + 38;
    int bar_w = vw_w - 28;
    gfx_fill_rounded_rect(bar_x, bar_y, bar_w, 8, 4, 0xFF313244);
    int fill_w = (bar_w * audio_volume) / 100;
    gfx_fill_rounded_rect(bar_x, bar_y, fill_w, 8, 4, audio_muted ? COLOR_RED_ACCENT : COLOR_BLUE_ACCENT);

    // Mute / Unmute Button
    int mb_y = vw_y + 56;
    gfx_fill_rounded_rect(bar_x, mb_y, bar_w, 28, 6, COLOR_OVERLAY);
    gfx_draw_rounded_rect(bar_x, mb_y, bar_w, 28, 6, COLOR_BORDER_LIGHT);
    gfx_draw_text(bar_x + 50, mb_y + 7, audio_muted ? "Unmute Audio" : "Mute Audio", audio_muted ? COLOR_GREEN_ACCENT : COLOR_RED_ACCENT);
}

static void draw_notifications_drawer(void) {
    int dw_w = 320;
    int dw_h = 420;
    int dw_x = SCREEN_WIDTH - dw_w - 12;
    int dw_y = 34;

    gfx_draw_shadow(dw_x, dw_y, dw_w, dw_h, 12, 140);
    gfx_fill_rounded_rect(dw_x, dw_y, dw_w, dw_h, 12, 0xFA181825);
    gfx_draw_rounded_rect(dw_x, dw_y, dw_w, dw_h, 12, COLOR_BORDER_LIGHT);

    // Header: "Notifications" and [Clear]
    icon_draw_16(dw_x + 16, dw_y + 12, ICON_SYSINFO, COLOR_LEMON_GOLD);
    gfx_draw_text(dw_x + 38, dw_y + 12, "Notification Center", COLOR_TEXT_PRIMARY);

    int clr_w = 64;
    int clr_x = dw_x + dw_w - clr_w - 14;
    gfx_fill_rounded_rect(clr_x, dw_y + 8, clr_w, 24, 6, COLOR_OVERLAY);
    gfx_draw_text(clr_x + 12, dw_y + 12, "Clear", COLOR_TEXT_MUTED);

    gfx_fill_rect(dw_x + 14, dw_y + 36, dw_w - 28, 1, COLOR_OVERLAY);

    // Recent notification items
    int count = alert_get_history_count();
    if (count == 0) {
        gfx_draw_text(dw_x + 40, dw_y + 80, "No new notifications.", COLOR_TEXT_MUTED);
        gfx_draw_text(dw_x + 40, dw_y + 98, "You're all caught up!", COLOR_TEXT_MUTED);
    } else {
        int max_show = (count > 5) ? 5 : count;
        for (int i = 0; i < max_show; i++) {
            alert_t* a = alert_get_history(count - 1 - i);
            if (!a) continue;

            int iy = dw_y + 46 + i * 68;
            gfx_fill_rounded_rect(dw_x + 12, iy, dw_w - 24, 60, 8, 0xFF1E1E2E);
            gfx_draw_rounded_rect(dw_x + 12, iy, dw_w - 24, 60, 8, COLOR_BORDER);

            uint32_t stripe_col = 0xFF38BDF8;
            if (a->type == ALERT_TYPE_SUCCESS) stripe_col = 0xFF34D399;
            else if (a->type == ALERT_TYPE_WARNING) stripe_col = 0xFFFBBF24;
            else if (a->type == ALERT_TYPE_ERROR) stripe_col = 0xFFF87171;

            gfx_fill_rounded_rect(dw_x + 14, iy + 4, 3, 52, 2, stripe_col);
            icon_draw_16(dw_x + 24, iy + 10, (icon_type_t)a->icon_id, stripe_col);

            gfx_draw_text(dw_x + 46, iy + 10, a->title, COLOR_TEXT_PRIMARY);

            char msg_tr[38];
            int m = 0;
            while (a->message[m] && m < 34) { msg_tr[m] = a->message[m]; m++; }
            msg_tr[m] = '\0';
            gfx_draw_text(dw_x + 46, iy + 28, msg_tr, COLOR_TEXT_SECONDARY);
        }
    }
}

static void draw_power_menu(void) {
    int pw_w = 170;
    int pw_h = 100;
    int pw_x = SCREEN_WIDTH - pw_w - 12;
    int pw_y = 34;

    gfx_draw_shadow(pw_x, pw_y, pw_w, pw_h, 8, 120);
    gfx_fill_rounded_rect(pw_x, pw_y, pw_w, pw_h, 8, 0xFA1E1E2E);
    gfx_draw_rounded_rect(pw_x, pw_y, pw_w, pw_h, 8, COLOR_BORDER_LIGHT);

    icon_draw_16(pw_x + 12, pw_y + 10, ICON_TERMINAL, COLOR_TEXT_PRIMARY);
    gfx_draw_text(pw_x + 34, pw_y + 10, "Exit to CLI", COLOR_TEXT_PRIMARY);
    gfx_fill_rect(pw_x + 8, pw_y + 32, pw_w - 16, 1, COLOR_OVERLAY);

    icon_draw_16(pw_x + 12, pw_y + 40, ICON_RESTART, COLOR_YELLOW_ACCENT);
    gfx_draw_text(pw_x + 34, pw_y + 40, "Restart System", COLOR_YELLOW_ACCENT);
    gfx_fill_rect(pw_x + 8, pw_y + 62, pw_w - 16, 1, COLOR_OVERLAY);

    icon_draw_16(pw_x + 12, pw_y + 70, ICON_POWER, COLOR_RED_ACCENT);
    gfx_draw_text(pw_x + 34, pw_y + 70, "Shut Down", COLOR_RED_ACCENT);
}

// Modern Taskbar Panel (Bottom: Pinned Apps + Open Window Tabs)
static void draw_taskbar(int mouse_x, int mouse_y) {
    int tb_x = 10;
    int tb_w = SCREEN_WIDTH - 20;
    int tb_h = 44;
    int tb_y = SCREEN_HEIGHT - tb_h - 4;

    // 1. Sleek, high-performance floating dock island (60 FPS optimized)
    gfx_fill_rounded_rect(tb_x, tb_y, tb_w, tb_h, 10, 0xF0141622);
    gfx_draw_rounded_rect(tb_x, tb_y, tb_w, tb_h, 10, 0x2EFFFFFF);
    for (int px = tb_x + 12; px < tb_x + tb_w - 12; px++) {
        gfx_put_pixel_alpha(px, tb_y + 1, 0xFFFFFF, 35);
    }

    // 2. Start Button on the left
    int start_hover = (mouse_x >= tb_x + 6 && mouse_x < tb_x + 92 && mouse_y >= tb_y + 4 && mouse_y < tb_y + tb_h - 4);
    if (start_menu_open || start_hover) {
        gfx_fill_rounded_rect(tb_x + 6, tb_y + 4, 84, tb_h - 8, 8, start_menu_open ? 0x40F9E2AF : COLOR_OVERLAY);
        gfx_draw_rounded_rect(tb_x + 6, tb_y + 4, 84, tb_h - 8, 8, start_menu_open ? COLOR_LEMON_GOLD : COLOR_BORDER_LIGHT);
        if (start_menu_open) {
            gfx_draw_glow_rect(tb_x + 6, tb_y + 4, 84, tb_h - 8, 8, COLOR_LEMON_GOLD, 2);
        }
    }
    icon_draw_24(tb_x + 12, tb_y + 10, ICON_LEMON_OS);
    gfx_draw_text(tb_x + 42, tb_y + 14, "Start", start_menu_open ? COLOR_LEMON_GOLD : COLOR_TEXT_PRIMARY);

    // Divider
    gfx_fill_rect(tb_x + 96, tb_y + 8, 1, tb_h - 16, COLOR_OVERLAY);

    // 3. Pinned Launchers
    int cur_x = tb_x + 104;
    int p_size = 36;
    for (size_t i = 0; i < NUM_PINNED_ITEMS; i++) {
        int is_hover = (mouse_x >= cur_x && mouse_x < cur_x + p_size && mouse_y >= tb_y + 4 && mouse_y < tb_y + tb_h - 4);
        if (is_hover) {
            gfx_fill_rounded_rect(cur_x, tb_y + 4, p_size, tb_h - 8, 8, COLOR_OVERLAY);
            gfx_draw_rounded_rect(cur_x, tb_y + 4, p_size, tb_h - 8, 8, COLOR_BORDER_LIGHT);
        }
        icon_draw_24(cur_x + 6, tb_y + 10, pinned_items[i].icon_id);

        // Check if pinned app has an active running window -> Draw neon indicator dot
        for (int w = 0; w < wm_get_window_count(); w++) {
            window_t* win = wm_get_window(w);
            if (win && !win->is_closed && win->icon_id == (int)pinned_items[i].icon_id) {
                if (win->is_active) {
                    // Glowing cyan indicator dot
                    gfx_fill_rounded_rect(cur_x + p_size / 2 - 4, tb_y + tb_h - 5, 8, 3, 1, 0xFF38BDF8);
                    gfx_put_pixel_alpha(cur_x + p_size / 2 - 5, tb_y + tb_h - 4, 0x38BDF8, 120);
                    gfx_put_pixel_alpha(cur_x + p_size / 2 + 4, tb_y + tb_h - 4, 0x38BDF8, 120);
                } else {
                    // Soft muted dot for background running app
                    gfx_fill_rounded_rect(cur_x + p_size / 2 - 2, tb_y + tb_h - 5, 4, 3, 1, 0xFFA6ADC8);
                }
                break;
            }
        }
        cur_x += p_size + 4;
    }

    // Divider
    gfx_fill_rect(cur_x + 2, tb_y + 8, 1, tb_h - 16, COLOR_OVERLAY);
    cur_x += 10;

    // 4. Open Window Tabs / Chips
    int win_count = wm_get_window_count();
    int tab_w = 140;
    int tab_h = 32;

    for (int w = 0; w < win_count; w++) {
        window_t* win = wm_get_window(w);
        if (!win || win->is_closed) continue;

        if (cur_x + tab_w > tb_x + tb_w - 70) break; // Don't overflow right edge controls

        int is_tab_hover = (mouse_x >= cur_x && mouse_x < cur_x + tab_w && mouse_y >= tb_y + 6 && mouse_y < tb_y + 6 + tab_h);

        uint32_t tab_bg = win->is_active ? 0xDD2A2B3D : (is_tab_hover ? 0xBB212234 : 0x88181825);
        gfx_fill_rounded_rect(cur_x, tb_y + 6, tab_w, tab_h, 7, tab_bg);

        if (win->is_active) {
            gfx_draw_rounded_rect(cur_x, tb_y + 6, tab_w, tab_h, 7, 0xFF38BDF8);
            // Glowing cyan active bottom line
            gfx_fill_rounded_rect(cur_x + 10, tb_y + 6 + tab_h - 3, tab_w - 20, 2, 1, 0xFF38BDF8);
        } else {
            gfx_draw_rounded_rect(cur_x, tb_y + 6, tab_w, tab_h, 7, is_tab_hover ? COLOR_BORDER_LIGHT : COLOR_OVERLAY);
        }

        // Window icon
        if (win->icon_id) {
            icon_draw_16(cur_x + 8, tb_y + 14, (icon_type_t)win->icon_id, COLOR_TEXT_PRIMARY);
        }

        uint32_t text_col = win->is_active ? COLOR_TEXT_PRIMARY : (win->is_minimized ? COLOR_TEXT_MUTED : COLOR_TEXT_SECONDARY);
        gfx_draw_text_ellipsis(cur_x + 28, tb_y + 14, tab_w - 36, win->title, text_col);

        cur_x += tab_w + 6;
    }

    // 5. Mission Control / Overview Button & Aero Peek at far right
    int sw_x = tb_x + tb_w - 56;
    int sw_hover = (mouse_x >= sw_x && mouse_x < sw_x + 36 && mouse_y >= tb_y + 4 && mouse_y < tb_y + tb_h - 4);
    if (wm_overview_is_open() || sw_hover) {
        gfx_fill_rounded_rect(sw_x, tb_y + 5, 36, tb_h - 10, 6, wm_overview_is_open() ? 0x4038BDF8 : COLOR_OVERLAY);
        gfx_draw_rounded_rect(sw_x, tb_y + 5, 36, tb_h - 10, 6, wm_overview_is_open() ? 0xFF38BDF8 : COLOR_BORDER_LIGHT);
    }
    icon_draw_16(sw_x + 10, tb_y + 14, ICON_RESTORE, wm_overview_is_open() ? 0xFF38BDF8 : COLOR_TEXT_MUTED);

    // Aero Peek glass strip at the very edge of the dock
    int peek_x = tb_x + tb_w - 14;
    int peek_hover = (mouse_x >= peek_x && mouse_x <= tb_x + tb_w && mouse_y >= tb_y + 4 && mouse_y < tb_y + tb_h - 4);
    gfx_fill_rounded_rect(peek_x, tb_y + 6, 8, tb_h - 12, 3, peek_hover ? COLOR_LEMON_GOLD : 0x40A6ADC8);
}

typedef struct {
    const char* name;
    const char* desc;
    icon_type_t icon_id;
    void (*launch)(void);
} start_app_entry_t;

static const start_app_entry_t start_app_grid[] = {
    {"Web Browser",     "NetSurf HTML & CSS Engine", ICON_FIREFOX,  gui_firefox_open},
    {"Terminal",        "LemonOS CLI Shell",     ICON_TERMINAL,     gui_terminal_open},
    {"File Explorer",   "Browse & Edit Files",   ICON_FILE_MANAGER, gui_files_open},
    {"Speedtest",       "Bandwidth & Latency",   ICON_WIFI,         gui_speedtest_open},
    {"Calculator",      "Standard Arithmetic",   ICON_CALCULATOR,   gui_calc_open},
    {"Unit Converter",  "Convert Units",         ICON_CONVERTER,    gui_converter_open},
    {"Clock & Timer",   "World Clock & Watch",   ICON_CLOCK,        gui_clock_open},
    {"About LemonOS",   "Hardware & Specs",      ICON_SYSINFO,      gui_sysinfo_open},
    {"Snake Game",      "Classic Snake Arcade",  ICON_GAMES,        gui_snake_open},
    {"Breakout Game",   "Brick Breaker Arcade",  ICON_GAMES,        gui_breakout_open},
    {"Pong Game",       "Table Tennis Arcade",   ICON_GAMES,        gui_pong_open},
    {"DOOM (1993)",     "Original id Tech 1",    ICON_GAMES,        app_doom_launch},
    {"Matrix Rain",     "Digital Rain Visual",   ICON_MATRIX,       gui_matrix_open},
    {"Services & Tasks","Init & Daemon Manager", ICON_SETTINGS,    gui_services_open}
};
#define NUM_START_APPS (sizeof(start_app_grid) / sizeof(start_app_grid[0]))

static void draw_start_menu(int mouse_x, int mouse_y) {
    int menu_w = 640;
    int menu_h = 440;
    int menu_x = (SCREEN_WIDTH - menu_w) / 2;
    int menu_y = 50;

    gfx_draw_shadow(menu_x, menu_y, menu_w, menu_h, 12, 140);
    gfx_fill_rounded_rect(menu_x, menu_y, menu_w, menu_h, 14, 0xFA1E1E2E);
    gfx_draw_rounded_rect(menu_x, menu_y, menu_w, menu_h, 14, COLOR_BORDER_LIGHT);

    // Search Box at top
    int sb_x = menu_x + 24;
    int sb_y = menu_y + 18;
    int sb_w = menu_w - 48;
    int sb_h = 36;
    gfx_fill_rounded_rect(sb_x, sb_y, sb_w, sb_h, 8, 0xFF181825);
    gfx_draw_rounded_rect(sb_x, sb_y, sb_w, sb_h, 8, COLOR_OVERLAY);
    icon_draw_16(sb_x + 12, sb_y + 10, ICON_SEARCH, COLOR_TEXT_MUTED);
    gfx_draw_text(sb_x + 36, sb_y + 10, "Search apps, files, settings...", COLOR_TEXT_MUTED);

    int num_apps = NUM_START_APPS;

    int cols = 3;
    int cell_w = (menu_w - 48 - (cols - 1) * 12) / cols;
    int cell_h = 56;
    int grid_y = sb_y + sb_h + 16;

    for (int i = 0; i < num_apps; i++) {
        int r = i / cols;
        int c = i % cols;
        int cx = sb_x + c * (cell_w + 12);
        int cy = grid_y + r * (cell_h + 10);

        int is_hover = (mouse_x >= cx && mouse_x < cx + cell_w && mouse_y >= cy && mouse_y < cy + cell_h);
        uint32_t cbg = is_hover ? 0xFF2F3146 : 0xFF181825;

        gfx_fill_rounded_rect(cx, cy, cell_w, cell_h, 8, cbg);
        gfx_draw_rounded_rect(cx, cy, cell_w, cell_h, 8, is_hover ? COLOR_LEMON_GOLD : COLOR_OVERLAY);
        icon_draw_24(cx + 10, cy + 16, start_app_grid[i].icon_id);
        gfx_draw_text_ellipsis(cx + 40, cy + 10, cell_w - 46, start_app_grid[i].name, COLOR_TEXT_PRIMARY);
        gfx_draw_text_ellipsis(cx + 40, cy + 28, cell_w - 46, start_app_grid[i].desc, COLOR_TEXT_MUTED);
    }

    // Bottom Bar: System actions
    int bot_y = menu_y + menu_h - 48;
    gfx_fill_rect(menu_x, bot_y, menu_w, 1, COLOR_OVERLAY);
    gfx_draw_text(menu_x + 24, bot_y + 14, "LemonOS 3.0 Enterprise", COLOR_TEXT_MUTED);

    int p_btn_w = 90;
    int p3_x = menu_x + menu_w - 24 - p_btn_w;
    int p2_x = p3_x - p_btn_w - 8;
    int p1_x = p2_x - p_btn_w - 8;

    int p1_hover = (mouse_x >= p1_x && mouse_x < p1_x + p_btn_w && mouse_y >= bot_y + 8 && mouse_y < bot_y + 36);
    int p2_hover = (mouse_x >= p2_x && mouse_x < p2_x + p_btn_w && mouse_y >= bot_y + 8 && mouse_y < bot_y + 36);
    int p3_hover = (mouse_x >= p3_x && mouse_x < p3_x + p_btn_w && mouse_y >= bot_y + 8 && mouse_y < bot_y + 36);

    gfx_fill_rounded_rect(p1_x, bot_y + 8, p_btn_w, 28, 6, p1_hover ? 0xFF2A2B3D : COLOR_OVERLAY);
    if (p1_hover) gfx_draw_rounded_rect(p1_x, bot_y + 8, p_btn_w, 28, 6, COLOR_BORDER_LIGHT);
    icon_draw_16(p1_x + 8, bot_y + 14, ICON_TERMINAL, COLOR_TEXT_SECONDARY);
    gfx_draw_text(p1_x + 30, bot_y + 14, "Exit CLI", COLOR_TEXT_SECONDARY);

    gfx_fill_rounded_rect(p2_x, bot_y + 8, p_btn_w, 28, 6, p2_hover ? 0xFF2A2B3D : COLOR_OVERLAY);
    if (p2_hover) gfx_draw_rounded_rect(p2_x, bot_y + 8, p_btn_w, 28, 6, COLOR_YELLOW_ACCENT);
    icon_draw_16(p2_x + 8, bot_y + 14, ICON_RESTART, COLOR_YELLOW_ACCENT);
    gfx_draw_text(p2_x + 30, bot_y + 14, "Restart", COLOR_YELLOW_ACCENT);

    gfx_fill_rounded_rect(p3_x, bot_y + 8, p_btn_w, 28, 6, p3_hover ? 0xFF2A2B3D : COLOR_OVERLAY);
    if (p3_hover) gfx_draw_rounded_rect(p3_x, bot_y + 8, p_btn_w, 28, 6, COLOR_RED_ACCENT);
    icon_draw_16(p3_x + 8, bot_y + 14, ICON_POWER, COLOR_RED_ACCENT);
    gfx_draw_text(p3_x + 30, bot_y + 14, "Power", COLOR_RED_ACCENT);
}

static void init_sample_wallpapers(void) {
    fs_mkdir("/system");
    fs_mkdir("/system/wallpapers");
    fs_mkdir("/home");
    fs_mkdir("/home/dwip");
    fs_mkdir("/home/dwip/Pictures");
    fs_mkdir("/downloads");

    // Create a real 64x48 sample 24-bit BMP file
    int w = 64, h = 48;
    int row_stride = ((24 * w + 31) / 32) * 4;
    uint32_t file_size = 54 + row_stride * h;
    static uint8_t sample_bmp[54 + 64 * 48 * 4];

    memset(sample_bmp, 0, 54);
    sample_bmp[0] = 'B'; sample_bmp[1] = 'M';
    *(uint32_t*)(sample_bmp + 2) = file_size;
    *(uint32_t*)(sample_bmp + 10) = 54;
    *(uint32_t*)(sample_bmp + 14) = 40;
    *(int32_t*)(sample_bmp + 18) = w;
    *(int32_t*)(sample_bmp + 22) = h;
    *(uint16_t*)(sample_bmp + 26) = 1;
    *(uint16_t*)(sample_bmp + 28) = 24;
    *(uint32_t*)(sample_bmp + 30) = 0;
    *(uint32_t*)(sample_bmp + 34) = row_stride * h;

    for (int y = 0; y < h; y++) {
        uint8_t* row = sample_bmp + 54 + y * row_stride;
        for (int x = 0; x < w; x++) {
            row[x * 3 + 0] = 30;
            row[x * 3 + 1] = 60 + ((x * 13 + y * 7) % 180);
            row[x * 3 + 2] = 20;
        }
    }
    fs_create_file("/system/wallpapers/matrix_cyber.bmp", (char*)sample_bmp);

    for (int y = 0; y < h; y++) {
        uint8_t* row = sample_bmp + 54 + y * row_stride;
        for (int x = 0; x < w; x++) {
            row[x * 3 + 0] = 80 + (y * 140) / h;
            row[x * 3 + 1] = 20 + (x * 40) / w;
            row[x * 3 + 2] = 120 + (y * 120) / h;
        }
    }
    fs_create_file("/system/wallpapers/sunset_vaporwave.bmp", (char*)sample_bmp);
    fs_create_file("/system/wallpapers/neon_aurora.bmp", (char*)sample_bmp);
    fs_create_file("/home/dwip/Pictures/cyberpunk.bmp", (char*)sample_bmp);
    fs_create_file("/downloads/sample_wallpaper.bmp", (char*)sample_bmp);
}

void desktop_init(void) {
    if (!gfx_init()) {
        return;
    }

    mouse_init(SCREEN_WIDTH, SCREEN_HEIGHT);
    wm_init();
    alert_init();
    init_sample_wallpapers();

    desktop_running = 1;
    start_menu_open = 0;
    power_menu_open = 0;
    calendar_open = 0;
    network_popup_open = 0;
    volume_popup_open = 0;
    notifications_open = 0;
    show_desktop_state = 0;
}

void desktop_run(void) {
    if (!desktop_running) {
        desktop_init();
    }

    while (desktop_running) {
        // Poll background network packets (DHCP, ARP, TCP)
        net_poll();

        // Update toast alert animations and timers
        alert_update();

        // 1. Process Keyboard Input
        poll_keyboard();
        while (has_key()) {
            char k = getchar();
            if (check_and_clear_alt_f4() || (is_alt_pressed() && (k == CHAR_F4 || k == 0x04))) {
                window_t* act = wm_get_active_window();
                if (act && !act->is_closed) {
                    wm_close_window(act);
                }
                continue;
            }
            if (k == 0x01) { // F1 - Toggle Start Menu
                start_menu_open = !start_menu_open;
            } else if (k == 0x03 || k == '`') { // F3 or ` - Toggle Mission Control Overview
                wm_overview_toggle();
            } else if (k == '\t') { // Tab - Window Switcher HUD
                if (!wm_switcher_is_open()) {
                    wm_switcher_open();
                } else {
                    wm_switcher_next();
                }
            } else if (k == 27) { // Esc - Close overlays
                start_menu_open = 0;
                power_menu_open = 0;
                calendar_open = 0;
                network_popup_open = 0;
                volume_popup_open = 0;
                notifications_open = 0;
                if (wm_switcher_is_open()) wm_switcher_close();
                if (wm_overview_is_open()) wm_overview_close();
            } else {
                wm_handle_key(k);
            }
        }

        // Advance smooth compositor animations (scale-ups, minimizes, restores)
        wm_update_animations();

        // 2. Process Mouse Input
        mouse_poll();
        int mx = mouse_get_x();
        int my = mouse_get_y();
        int btn = mouse_get_buttons();
        int clicked = mouse_is_left_clicked();
        int released = mouse_is_left_released();
        int scroll_dz = mouse_get_scroll_delta();

        // Handle active desktop icon dragging & marquee updates while mouse button held
        if (btn & 1) {
            if (dragging_icon_idx >= 0) {
                int cur_dx = mx - icon_drag_start_x;
                int cur_dy = my - icon_drag_start_y;
                if (cur_dx < -16 || cur_dx > 16 || cur_dy < -16 || cur_dy > 16) {
                    icon_has_moved = 1;
                }
                int new_ix = mx - icon_drag_off_x;
                int new_iy = my - icon_drag_off_y;
                if (new_ix < 10) new_ix = 10;
                if (new_ix > SCREEN_WIDTH - 80) new_ix = SCREEN_WIDTH - 80;
                if (new_iy < 36) new_iy = 36;
                if (new_iy > SCREEN_HEIGHT - 120) new_iy = SCREEN_HEIGHT - 120;
                desktop_icons[dragging_icon_idx].x = new_ix;
                desktop_icons[dragging_icon_idx].y = new_iy;
                goto input_handled;
            } else if (marquee_active) {
                marquee_cur_x = mx;
                marquee_cur_y = my;
                goto input_handled;
            }
        }

        if (released || !(btn & 1)) {
            if (dragging_icon_idx >= 0) {
                if (!icon_has_moved) {
                    desktop_icons[dragging_icon_idx].launch();
                }
                dragging_icon_idx = -1;
            }
            if (marquee_active) {
                marquee_active = 0;
            }
        }

        // 3. Handle Clicks on Alerts & System UI
        if (alert_handle_mouse(mx, my, clicked)) {
            goto input_handled;
        }

        // Mission Control / Overview mode handles clicks
        if (wm_overview_is_open()) {
            if (wm_overview_handle_mouse(mx, my, clicked)) {
                goto input_handled;
            }
        }

        if (clicked) {
            // Close switcher if clicked outside
            if (wm_switcher_is_open()) {
                wm_switcher_close();
                goto input_handled;
            }

            // Check Start Menu Clicks
            if (start_menu_open) {
                int menu_w = 640;
                int menu_h = 440;
                int menu_x = (SCREEN_WIDTH - menu_w) / 2;
                int menu_y = 50;

                if (mx >= menu_x && mx < menu_x + menu_w && my >= menu_y && my < menu_y + menu_h) {
                    int bot_y = menu_y + menu_h - 48;
                    int p_btn_w = 90;
                    int p3_x = menu_x + menu_w - 24 - p_btn_w;
                    int p2_x = p3_x - p_btn_w - 8;
                    int p1_x = p2_x - p_btn_w - 8;

                    if (my >= bot_y + 8 && my < bot_y + 36) {
                        if (mx >= p1_x && mx < p1_x + p_btn_w) {
                            desktop_exit();
                            return;
                        } else if (mx >= p2_x && mx < p2_x + p_btn_w) {
                            reboot();
                            return;
                        } else if (mx >= p3_x && mx < p3_x + p_btn_w) {
                            shutdown();
                            return;
                        }
                    }

                    int sb_x = menu_x + 24;
                    int sb_y = menu_y + 18;
                    int sb_h = 36;
                    int cols = 3;
                    int cell_w = (menu_w - 48 - (cols - 1) * 12) / cols;
                    int cell_h = 56;
                    int grid_y = sb_y + sb_h + 16;

                    for (int i = 0; i < (int)NUM_START_APPS; i++) {
                        int r = i / cols;
                        int c = i % cols;
                        int cx = sb_x + c * (cell_w + 12);
                        int cy = grid_y + r * (cell_h + 10);
                        if (mx >= cx && mx < cx + cell_w && my >= cy && my < cy + cell_h) {
                            start_menu_open = 0;
                            if (start_app_grid[i].launch) start_app_grid[i].launch();
                            goto input_handled;
                        }
                    }
                    goto input_handled;
                } else {
                    start_menu_open = 0;
                }
            }

            // Check Notifications Drawer Clicks
            if (notifications_open) {
                int dw_w = 320;
                int dw_h = 420;
                int dw_x = SCREEN_WIDTH - dw_w - 12;
                int dw_y = 34;
                if (mx >= dw_x && mx < dw_x + dw_w && my >= dw_y && my < dw_y + dw_h) {
                    // Check Clear button
                    int clr_w = 64;
                    int clr_x = dw_x + dw_w - clr_w - 14;
                    if (mx >= clr_x && mx < clr_x + clr_w && my >= dw_y + 8 && my < dw_y + 32) {
                        alert_clear_history();
                    }
                    goto input_handled;
                } else {
                    notifications_open = 0;
                }
            }

            // Check Network Popup Clicks
            if (network_popup_open) {
                int nw_w = 300;
                int nw_h = 240;
                int nw_x = SCREEN_WIDTH - nw_w - 12;
                int nw_y = 34;
                if (mx >= nw_x && mx < nw_x + nw_w && my >= nw_y && my < nw_y + nw_h) {
                    int btn_w = (nw_w - 40) / 2;
                    int btn_y = nw_y + nw_h - 40;
                    if (my >= btn_y && my < btn_y + 28) {
                        if (mx >= nw_x + 16 && mx < nw_x + 16 + btn_w) {
                            // Ping Gateway
                            net_config_t cfg;
                            net_get_config(&cfg);
                            net_ping(cfg.gateway, 1, 1000);
                            alert_send("Ping Gateway", "Reply from 10.0.2.2: time<1ms", ICON_WIFI, ALERT_TYPE_SUCCESS);
                        } else if (mx >= nw_x + 24 + btn_w && mx < nw_x + 24 + btn_w * 2) {
                            // Renew DHCP
                            net_dhcp_request();
                            alert_send("DHCP Renewed", "Assigned IP 10.0.2.15", ICON_WIFI, ALERT_TYPE_INFO);
                        }
                    }
                    goto input_handled;
                } else {
                    network_popup_open = 0;
                }
            }

            // Check Volume Popup Clicks
            if (volume_popup_open) {
                int vw_w = 220;
                int vw_h = 100;
                int vw_x = SCREEN_WIDTH - vw_w - 60;
                int vw_y = 34;
                if (mx >= vw_x && mx < vw_x + vw_w && my >= vw_y && my < vw_y + vw_h) {
                    int bar_x = vw_x + 14;
                    int mb_y = vw_y + 56;
                    int bar_w = vw_w - 28;
                    if (my >= mb_y && my < mb_y + 28 && mx >= bar_x && mx < bar_x + bar_w) {
                        audio_muted = !audio_muted;
                        alert_send(audio_muted ? "Audio Muted" : "Audio Unmuted", "Sound Blaster 16", ICON_SPEAKER, ALERT_TYPE_INFO);
                    }
                    goto input_handled;
                } else {
                    volume_popup_open = 0;
                }
            }

            // Check Calendar Popup Clicks
            if (calendar_open) {
                int cw_w = 280;
                int cw_h = 240;
                int cw_x = (SCREEN_WIDTH - cw_w) / 2;
                int cw_y = 34;
                if (mx >= cw_x && mx < cw_x + cw_w && my >= cw_y && my < cw_y + cw_h) {
                    goto input_handled;
                } else {
                    calendar_open = 0;
                }
            }

            // Check Power Menu Clicks
            if (power_menu_open) {
                int pw_w = 170;
                int pw_h = 100;
                int pw_x = SCREEN_WIDTH - pw_w - 12;
                int pw_y = 34;
                if (mx >= pw_x && mx < pw_x + pw_w && my >= pw_y && my < pw_y + pw_h) {
                    if (my >= pw_y && my < pw_y + 32) {
                        desktop_exit();
                        return;
                    } else if (my >= pw_y + 32 && my < pw_y + 62) {
                        reboot();
                        return;
                    } else if (my >= pw_y + 62 && my < pw_y + pw_h) {
                        shutdown();
                        return;
                    }
                } else {
                    power_menu_open = 0;
                }
            }

            // Check Top Bar Clicks
            if (my < 30) {
                // Activities button -> Toggle Mission Control Overview
                if (mx >= 6 && mx < 118) {
                    wm_overview_toggle();
                    goto input_handled;
                }
                // Center Clock -> Toggle Calendar Popup
                int cw_x = (SCREEN_WIDTH - 180) / 2;
                if (mx >= cw_x && mx < cw_x + 180) {
                    calendar_open = !calendar_open;
                    goto input_handled;
                }
                // Audio Volume Pill
                int vol_x = SCREEN_WIDTH - 170;
                if (mx >= vol_x && mx < vol_x + 28) {
                    volume_popup_open = !volume_popup_open;
                    goto input_handled;
                }
                // Network Pill
                int net_x = SCREEN_WIDTH - 138;
                if (mx >= net_x && mx < net_x + 28) {
                    network_popup_open = !network_popup_open;
                    goto input_handled;
                }
                // Notification Bell Pill
                int notif_x = SCREEN_WIDTH - 106;
                if (mx >= notif_x && mx < notif_x + 36) {
                    notifications_open = !notifications_open;
                    alert_mark_all_read();
                    goto input_handled;
                }
                // Power menu
                int pwr_x = SCREEN_WIDTH - 66;
                if (mx >= pwr_x) {
                    power_menu_open = !power_menu_open;
                    goto input_handled;
                }
                goto input_handled;
            }

            // Check Taskbar Clicks (Floating Acrylic Dock)
            int tb_x = 10;
            int tb_w = SCREEN_WIDTH - 20;
            int tb_h = 44;
            int tb_y = SCREEN_HEIGHT - tb_h - 4;
            if (my >= tb_y) {
                // Start button
                if (mx >= tb_x + 6 && mx < tb_x + 92) {
                    start_menu_open = !start_menu_open;
                    goto input_handled;
                }

                // Pinned app launchers
                int cur_x = tb_x + 104;
                int p_size = 36;
                for (size_t i = 0; i < NUM_PINNED_ITEMS; i++) {
                    if (mx >= cur_x && mx < cur_x + p_size) {
                        pinned_items[i].launch();
                        goto input_handled;
                    }
                    cur_x += p_size + 4;
                }

                // Open window tabs
                cur_x += 10;
                int win_count = wm_get_window_count();
                int tab_w = 140;
                int tab_h = 32;

                for (int w = 0; w < win_count; w++) {
                    window_t* win = wm_get_window(w);
                    if (!win || win->is_closed) continue;
                    if (cur_x + tab_w > tb_x + tb_w - 70) break;

                    if (mx >= cur_x && mx < cur_x + tab_w && my >= tb_y + 6 && my < tb_y + 6 + tab_h) {
                        if (win->is_active) {
                            wm_minimize_window(win);
                        } else if (win->is_minimized) {
                            wm_restore_window(win);
                        } else {
                            wm_bring_to_front(win);
                        }
                        goto input_handled;
                    }
                    cur_x += tab_w + 6;
                }

                // Window Switcher / Mission Control Button
                int sw_x = tb_x + tb_w - 56;
                if (mx >= sw_x && mx < sw_x + 36) {
                    wm_overview_toggle();
                    goto input_handled;
                }

                // Aero Peek ("Show Desktop") at far right
                int peek_x = tb_x + tb_w - 14;
                if (mx >= peek_x && mx <= tb_x + tb_w) {
                    if (!show_desktop_state) {
                        wm_minimize_all();
                        show_desktop_state = 1;
                    } else {
                        wm_restore_all();
                        show_desktop_state = 0;
                    }
                    goto input_handled;
                }
                goto input_handled;
            }

            // Check Desktop Icons Clicks ONLY if no window covers this position
            if (!wm_has_window_at(mx, my)) {
                for (size_t i = 0; i < NUM_DESKTOP_ICONS; i++) {
                    int ix = desktop_icons[i].x;
                    int iy = desktop_icons[i].y;
                    gui_rect_t box;
                    gui_get_desktop_icon_box(ix, iy, &box);
                    if (gui_hitbox_contains(box.x, box.y, box.w, box.h, mx, my)) {
                        dragging_icon_idx = (int)i;
                        icon_drag_start_x = mx;
                        icon_drag_start_y = my;
                        icon_drag_off_x = mx - ix;
                        icon_drag_off_y = my - iy;
                        icon_has_moved = 0;
                        goto input_handled;
                    }
                }

                // Clicked on empty desktop background: start marquee selection!
                if (my >= 30 && my < SCREEN_HEIGHT - 50) {
                    marquee_active = 1;
                    marquee_start_x = mx;
                    marquee_start_y = my;
                    marquee_cur_x = mx;
                    marquee_cur_y = my;
                }
            }
        }

        // Forward to window manager (handles dragging, snapping, resizing, client clicks, scrolling)
        wm_handle_mouse(mx, my, btn, clicked, released, scroll_dz);

    input_handled:

        // 4. Render Everything to Backbuffer
        draw_wallpaper();
        draw_desktop_icons(mx, my);

        // Draw desktop rubberband marquee selection
        if (marquee_active) {
            int x1 = (marquee_start_x < marquee_cur_x) ? marquee_start_x : marquee_cur_x;
            int y1 = (marquee_start_y < marquee_cur_y) ? marquee_start_y : marquee_cur_y;
            int w = (marquee_start_x > marquee_cur_x) ? (marquee_start_x - marquee_cur_x) : (marquee_cur_x - marquee_start_x);
            int h = (marquee_start_y > marquee_cur_y) ? (marquee_start_y - marquee_cur_y) : (marquee_cur_y - marquee_start_y);
            if (w > 2 && h > 2) {
                gfx_fill_rect_alpha(x1, y1, w, h, COLOR_BLUE_ACCENT, 45);
                gfx_draw_rect(x1, y1, w, h, COLOR_BLUE_ACCENT);
            }
        }

        // Window compositor snap preview highlight overlay
        wm_draw_snap_preview();

        // Render all windows (with shadows, active titlebar gradient, and resize handles)
        wm_render_windows();

        // Taskbar & Top Bar Panels
        draw_taskbar(mx, my);
        draw_top_bar(mx, my);

        // Background Toast Alerts
        alert_render();

        // System Overlays & Popups (Render in front of alerts)
        if (calendar_open)       draw_calendar_popup();
        if (network_popup_open)  draw_network_popup();
        if (volume_popup_open)   draw_volume_popup();
        if (notifications_open)  draw_notifications_drawer();
        if (start_menu_open)     draw_start_menu(mx, my);
        if (power_menu_open)     draw_power_menu();

        // Mission Control / Overview Grid (renders above desktop & windows)
        if (wm_overview_is_open()) {
            wm_draw_overview(mx, my);
        }

        // Alt-Tab Window Switcher HUD Modal
        wm_draw_switcher();

        // Hardware Mouse Cursor
        gfx_draw_cursor(mx, my);

        // 5. Flip Backbuffer to VRAM (Optimized REP MOVSD blit)
        gfx_flip();

        // 6. Smooth 60 FPS Frame Rate Management
        static uint32_t last_frame_ms = 0;
        uint32_t cur_ms = timer_get_ticks_ms();
        if (cur_ms >= last_frame_ms && (cur_ms - last_frame_ms) < 16) {
            uint32_t remain_ms = 16 - (cur_ms - last_frame_ms);
            if (remain_ms > 0 && remain_ms <= 16) {
                timer_sleep_ms(remain_ms);
            }
        }
        last_frame_ms = timer_get_ticks_ms();
    }
}

