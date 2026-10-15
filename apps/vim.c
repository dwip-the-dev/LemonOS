// Vim 9.1 Modal Text Editor for LemonOS
// Author: Dwip (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "vim.h"
#include "vga.h"
#include "keyboard.h"
#include "filesystem.h"
#include "vfs.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

#define MAX_LINES 512
#define MAX_LINE_LEN 160
#define VIM_ROWS 23

typedef enum {
    VIM_MODE_NORMAL,
    VIM_MODE_INSERT,
    VIM_MODE_COMMAND
} vim_mode_t;

static char lines[MAX_LINES][MAX_LINE_LEN];
static int line_count = 0;
static int cur_line = 0;
static int cur_col = 0;
static int top_line = 0;
static int modified = 0;
static int show_line_numbers = 1;
static char current_file[128] = "";
static char yank_buffer[MAX_LINE_LEN] = "";
static char cmd_buf[64] = "";
static int cmd_len = 0;
static char status_msg[80] = "";
static vim_mode_t mode = VIM_MODE_NORMAL;

static void vim_render(void) {
    int gutter_w = show_line_numbers ? 5 : 0;
    int text_w = VGA_WIDTH - gutter_w;

    for (int r = 0; r < VIM_ROWS; r++) {
        int lidx = top_line + r;
        vga_set_cursor(0, r);

        if (lidx < line_count) {
            // Draw gutter line number
            if (show_line_numbers) {
                char gbuf[16];
                snprintf(gbuf, sizeof(gbuf), "%3d ", lidx + 1);
                vga_print_color(gbuf, VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
            }
            // Draw line text
            const char* ltext = lines[lidx];
            int llen = strlen(ltext);
            for (int c = 0; c < text_w; c++) {
                char ch = (c < llen) ? ltext[c] : ' ';
                vga_putchar_color(ch, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            }
        } else {
            // Empty buffer line marked with Vim's signature '~'
            if (show_line_numbers) {
                vga_print_color("    ", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
            }
            vga_putchar_color('~', VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
            for (int c = 1; c < text_w; c++) {
                vga_putchar_color(' ', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            }
        }
    }

    // Status bar (Row 23)
    char stat[80];
    if (mode == VIM_MODE_INSERT) {
        snprintf(stat, sizeof(stat), "-- INSERT --                         %d,%d", cur_line + 1, cur_col + 1);
        vga_set_cursor(0, 23);
        vga_print_color(stat, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        for (int c = strlen(stat); c < VGA_WIDTH; c++) vga_putchar_color(' ', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    } else if (mode == VIM_MODE_COMMAND) {
        char cbar[80];
        snprintf(cbar, sizeof(cbar), ":%s", cmd_buf);
        vga_set_cursor(0, 23);
        vga_print_color(cbar, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        for (int c = strlen(cbar); c < VGA_WIDTH; c++) vga_putchar_color(' ', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    } else {
        if (status_msg[0]) {
            snprintf(stat, sizeof(stat), "%s", status_msg);
        } else {
            snprintf(stat, sizeof(stat), "\"%s\" %s%dL, %dC   [%d/%d, %d]",
                     current_file[0] ? current_file : "[No Name]",
                     modified ? "[+] " : "",
                     line_count,
                     cur_line + 1,
                     cur_line + 1, line_count, cur_col + 1);
        }
        vga_set_cursor(0, 23);
        vga_print_color(stat, VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
        for (int c = strlen(stat); c < VGA_WIDTH; c++) vga_putchar_color(' ', VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
    }

    // Position cursor
    if (mode == VIM_MODE_COMMAND) {
        vga_set_cursor(1 + cmd_len, 23);
    } else {
        int scr_y = cur_line - top_line;
        int scr_x = gutter_w + cur_col;
        if (scr_x >= VGA_WIDTH) scr_x = VGA_WIDTH - 1;
        vga_set_cursor(scr_x, scr_y);
    }
}

static int vim_save_file(void) {
    if (!current_file[0]) {
        snprintf(status_msg, sizeof(status_msg), "E32: No file name");
        return -1;
    }
    int total_bytes = 0;
    for (int i = 0; i < line_count; i++) total_bytes += strlen(lines[i]) + 1;
    char* buf = (char*)malloc(total_bytes + 2);
    if (!buf) return -1;
    buf[0] = '\0';
    for (int i = 0; i < line_count; i++) {
        strcat(buf, lines[i]);
        strcat(buf, "\n");
    }
    int ret = fs_create_file(current_file, buf);
    free(buf);
    if (ret == 0) {
        modified = 0;
        snprintf(status_msg, sizeof(status_msg), "\"%s\" %dL, %dB written", current_file, line_count, total_bytes);
        return 0;
    } else {
        snprintf(status_msg, sizeof(status_msg), "E212: Can't open file for writing");
        return -1;
    }
}

int vim_main(int argc, char** argv) {
    line_count = 0;
    cur_line = 0;
    cur_col = 0;
    top_line = 0;
    modified = 0;
    mode = VIM_MODE_NORMAL;
    status_msg[0] = '\0';
    current_file[0] = '\0';

    if (argc > 1 && argv[1]) {
        strncpy(current_file, argv[1], sizeof(current_file) - 1);
        char* content = fs_read_file(current_file);
        if (content) {
            const char* p = content;
            while (*p && line_count < MAX_LINES) {
                int c = 0;
                while (*p && *p != '\n' && *p != '\r' && c < MAX_LINE_LEN - 1) {
                    lines[line_count][c++] = *p++;
                }
                lines[line_count][c] = '\0';
                line_count++;
                if (*p == '\r') p++;
                if (*p == '\n') p++;
            }
            snprintf(status_msg, sizeof(status_msg), "\"%s\" %dL", current_file, line_count);
        } else {
            snprintf(status_msg, sizeof(status_msg), "\"%s\" [New File]", current_file);
        }
    }

    if (line_count == 0) {
        lines[0][0] = '\0';
        line_count = 1;
    }

    vga_clear();

    while (1) {
        vim_render();

        poll_keyboard();
        while (!has_key()) {
            poll_keyboard();
        }
        char ch = getchar();

        if (mode == VIM_MODE_NORMAL) {
            status_msg[0] = '\0';
            // Mode switching
            if (ch == 'i') {
                mode = VIM_MODE_INSERT;
            } else if (ch == 'a') {
                int len = strlen(lines[cur_line]);
                if (cur_col < len) cur_col++;
                mode = VIM_MODE_INSERT;
            } else if (ch == 'o') {
                // Open new line below
                if (line_count < MAX_LINES - 1) {
                    for (int i = line_count; i > cur_line + 1; i--) strcpy(lines[i], lines[i - 1]);
                    lines[cur_line + 1][0] = '\0';
                    line_count++;
                    cur_line++;
                    cur_col = 0;
                    if (cur_line >= top_line + VIM_ROWS) top_line = cur_line - VIM_ROWS + 1;
                    modified = 1;
                    mode = VIM_MODE_INSERT;
                }
            } else if (ch == 'O') {
                // Open new line above
                if (line_count < MAX_LINES - 1) {
                    for (int i = line_count; i > cur_line; i--) strcpy(lines[i], lines[i - 1]);
                    lines[cur_line][0] = '\0';
                    line_count++;
                    cur_col = 0;
                    modified = 1;
                    mode = VIM_MODE_INSERT;
                }
            } else if (ch == ':') {
                mode = VIM_MODE_COMMAND;
                cmd_len = 0;
                cmd_buf[0] = '\0';
            }
            // Navigation (h, j, k, l and arrows)
            else if (ch == 'h' || ch == CHAR_LEFT) {
                if (cur_col > 0) cur_col--;
            } else if (ch == 'l' || ch == CHAR_RIGHT) {
                int len = strlen(lines[cur_line]);
                if (cur_col < len - 1) cur_col++;
            } else if (ch == 'j' || ch == CHAR_DOWN) {
                if (cur_line < line_count - 1) cur_line++;
                int len = strlen(lines[cur_line]);
                if (cur_col > len) cur_col = len > 0 ? len - 1 : 0;
                if (cur_line >= top_line + VIM_ROWS) top_line = cur_line - VIM_ROWS + 1;
            } else if (ch == 'k' || ch == CHAR_UP) {
                if (cur_line > 0) cur_line--;
                int len = strlen(lines[cur_line]);
                if (cur_col > len) cur_col = len > 0 ? len - 1 : 0;
                if (cur_line < top_line) top_line = cur_line;
            } else if (ch == '0' || ch == CHAR_HOME) {
                cur_col = 0;
            } else if (ch == '$' || ch == CHAR_END) {
                int len = strlen(lines[cur_line]);
                cur_col = len > 0 ? len - 1 : 0;
            } else if (ch == 'G') {
                cur_line = line_count - 1;
                cur_col = 0;
                if (cur_line >= top_line + VIM_ROWS) top_line = cur_line - VIM_ROWS + 1;
            }
            // Actions
            else if (ch == 'x') { // Delete char under cursor
                int len = strlen(lines[cur_line]);
                if (len > 0 && cur_col < len) {
                    for (int i = cur_col; i < len; i++) lines[cur_line][i] = lines[cur_line][i + 1];
                    modified = 1;
                }
            } else if (ch == 'd') { // Check for 'dd' line deletion
                poll_keyboard();
                while (!has_key()) poll_keyboard();
                char next_ch = getchar();
                if (next_ch == 'd') {
                    strcpy(yank_buffer, lines[cur_line]);
                    if (line_count > 1) {
                        for (int i = cur_line; i < line_count - 1; i++) strcpy(lines[i], lines[i + 1]);
                        line_count--;
                        if (cur_line >= line_count) cur_line = line_count - 1;
                    } else {
                        lines[0][0] = '\0';
                    }
                    modified = 1;
                    snprintf(status_msg, sizeof(status_msg), "1 line deleted");
                }
            } else if (ch == 'y') { // Check for 'yy' yank
                poll_keyboard();
                while (!has_key()) poll_keyboard();
                char next_ch = getchar();
                if (next_ch == 'y') {
                    strcpy(yank_buffer, lines[cur_line]);
                    snprintf(status_msg, sizeof(status_msg), "1 line yanked");
                }
            } else if (ch == 'p') { // Put / paste yanked line
                if (yank_buffer[0] && line_count < MAX_LINES - 1) {
                    for (int i = line_count; i > cur_line + 1; i--) strcpy(lines[i], lines[i - 1]);
                    strcpy(lines[cur_line + 1], yank_buffer);
                    line_count++;
                    cur_line++;
                    modified = 1;
                }
            }
        } else if (mode == VIM_MODE_INSERT) {
            if (ch == 27) { // Escape returns to NORMAL mode
                mode = VIM_MODE_NORMAL;
                if (cur_col > 0) cur_col--;
            } else if (ch == '\b' || ch == 127) {
                if (cur_col > 0) {
                    int len = strlen(lines[cur_line]);
                    for (int i = cur_col - 1; i < len; i++) lines[cur_line][i] = lines[cur_line][i + 1];
                    cur_col--;
                    modified = 1;
                }
            } else if (ch == '\n' || ch == '\r') {
                if (line_count < MAX_LINES - 1) {
                    for (int i = line_count; i > cur_line + 1; i--) strcpy(lines[i], lines[i - 1]);
                    strcpy(lines[cur_line + 1], &lines[cur_line][cur_col]);
                    lines[cur_line][cur_col] = '\0';
                    line_count++;
                    cur_line++;
                    cur_col = 0;
                    if (cur_line >= top_line + VIM_ROWS) top_line = cur_line - VIM_ROWS + 1;
                    modified = 1;
                }
            } else if ((unsigned char)ch >= 32 && (unsigned char)ch <= 126) {
                int len = strlen(lines[cur_line]);
                if (len < MAX_LINE_LEN - 2) {
                    for (int i = len; i >= cur_col; i--) lines[cur_line][i + 1] = lines[cur_line][i];
                    lines[cur_line][cur_col] = ch;
                    cur_col++;
                    modified = 1;
                }
            }
        } else if (mode == VIM_MODE_COMMAND) {
            if (ch == 27) { // Escape cancels command mode
                mode = VIM_MODE_NORMAL;
                cmd_len = 0;
                cmd_buf[0] = '\0';
            } else if (ch == '\b' || ch == 127) {
                if (cmd_len > 0) {
                    cmd_len--;
                    cmd_buf[cmd_len] = '\0';
                } else {
                    mode = VIM_MODE_NORMAL;
                }
            } else if (ch == '\n' || ch == '\r') {
                // Execute command
                mode = VIM_MODE_NORMAL;
                if (strcmp(cmd_buf, "w") == 0) {
                    vim_save_file();
                } else if (strcmp(cmd_buf, "q") == 0) {
                    if (modified) {
                        snprintf(status_msg, sizeof(status_msg), "E37: No write since last change (add ! to override)");
                    } else {
                        break;
                    }
                } else if (strcmp(cmd_buf, "q!") == 0) {
                    break;
                } else if (strcmp(cmd_buf, "wq") == 0 || strcmp(cmd_buf, "x") == 0) {
                    if (vim_save_file() == 0) break;
                } else if (strcmp(cmd_buf, "set nu") == 0) {
                    show_line_numbers = 1;
                } else if (strcmp(cmd_buf, "set nonu") == 0) {
                    show_line_numbers = 0;
                } else if (strncmp(cmd_buf, "w ", 2) == 0) {
                    strncpy(current_file, &cmd_buf[2], sizeof(current_file) - 1);
                    vim_save_file();
                } else {
                    snprintf(status_msg, sizeof(status_msg), "E492: Not an editor command: %s", cmd_buf);
                }
                cmd_len = 0;
                cmd_buf[0] = '\0';
            } else if (cmd_len < (int)sizeof(cmd_buf) - 2 && (unsigned char)ch >= 32 && (unsigned char)ch <= 126) {
                cmd_buf[cmd_len++] = ch;
                cmd_buf[cmd_len] = '\0';
            }
        }
    }

    vga_clear();
    return 0;
}
