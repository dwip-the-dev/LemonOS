// GNU nano Terminal Editor for LemonOS
// Author: Dwip (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "nano.h"
#include "vga.h"
#include "keyboard.h"
#include "vfs.h"
#include "filesystem.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

#define MAX_LINES 512
#define MAX_LINE_LEN 160
#define EDIT_ROWS 22

static char lines[MAX_LINES][MAX_LINE_LEN];
static int line_count = 0;
static int cur_line = 0;
static int cur_col = 0;
static int top_line = 0;
static int modified = 0;
static char current_file[128] = "";
static char status_msg[80] = "";

static void nano_draw_bar(int y, const char* text, uint8_t fg, uint8_t bg) {
    int len = strlen(text);
    for (int x = 0; x < VGA_WIDTH; x++) {
        char c = (x < len) ? text[x] : ' ';
        vga_set_cursor(x, y);
        vga_putchar_color(c, fg, bg);
    }
}

static void nano_render(void) {
    // Top Title Bar
    char title[80];
    snprintf(title, sizeof(title), "  GNU nano 7.2          File: %s%s", 
             current_file[0] ? current_file : "New Buffer", 
             modified ? " [Modified]" : "");
    nano_draw_bar(0, title, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);

    // Edit Area (rows 1..EDIT_ROWS)
    for (int r = 0; r < EDIT_ROWS; r++) {
        int lidx = top_line + r;
        vga_set_cursor(0, r + 1);
        if (lidx < line_count) {
            const char* ltext = lines[lidx];
            int llen = strlen(ltext);
            for (int c = 0; c < VGA_WIDTH; c++) {
                char ch = (c < llen) ? ltext[c] : ' ';
                vga_putchar_color(ch, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            }
        } else {
            // Empty space
            for (int c = 0; c < VGA_WIDTH; c++) {
                vga_putchar_color(' ', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            }
        }
    }

    // Status Message Bar (Row 23)
    nano_draw_bar(23, status_msg[0] ? status_msg : "", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);

    // Bottom Help Bar (Row 24)
    const char* shortcuts = " ^G Help    ^O WriteOut    ^R Read File    ^K Cut Text    ^X Exit";
    nano_draw_bar(24, shortcuts, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_CYAN);

    // Position cursor
    int scr_y = 1 + (cur_line - top_line);
    int scr_x = cur_col;
    if (scr_x >= VGA_WIDTH) scr_x = VGA_WIDTH - 1;
    vga_set_cursor(scr_x, scr_y);
}

static void nano_save(void) {
    if (!current_file[0]) {
        snprintf(status_msg, sizeof(status_msg), "[ Error: No filename specified ]");
        return;
    }
    // Calculate total buffer size
    int total_bytes = 0;
    for (int i = 0; i < line_count; i++) {
        total_bytes += strlen(lines[i]) + 1; // +1 for '\n'
    }
    char* buf = (char*)malloc(total_bytes + 2);
    if (!buf) {
        snprintf(status_msg, sizeof(status_msg), "[ Memory allocation failed ]");
        return;
    }
    buf[0] = '\0';
    for (int i = 0; i < line_count; i++) {
        strcat(buf, lines[i]);
        strcat(buf, "\n");
    }

    // Save to LemonFS / VFS
    if (fs_create_file(current_file, buf) == 0) {
        modified = 0;
        snprintf(status_msg, sizeof(status_msg), "[ Wrote %d lines to %s ]", line_count, current_file);
    } else {
        snprintf(status_msg, sizeof(status_msg), "[ Error writing to %s ]", current_file);
    }
    free(buf);
}

int nano_main(int argc, char** argv) {
    line_count = 0;
    cur_line = 0;
    cur_col = 0;
    top_line = 0;
    modified = 0;
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
            snprintf(status_msg, sizeof(status_msg), "[ Read %d lines from %s ]", line_count, current_file);
        } else {
            snprintf(status_msg, sizeof(status_msg), "[ New File ]");
        }
    } else {
        snprintf(status_msg, sizeof(status_msg), "[ New Buffer ]");
    }

    if (line_count == 0) {
        lines[0][0] = '\0';
        line_count = 1;
    }

    vga_clear();

    while (1) {
        nano_render();

        poll_keyboard();
        while (!has_key()) {
            poll_keyboard();
        }
        char ch = getchar();

        // Check for Ctrl key combinations
        if (is_ctrl_pressed()) {
            if (ch == 'x' || ch == 'X' || ch == 24) { // Ctrl+X
                break;
            }
            if (ch == 'o' || ch == 'O' || ch == 15) { // Ctrl+O WriteOut
                nano_save();
                continue;
            }
        }

        // Navigation
        if (ch == CHAR_UP) {
            if (cur_line > 0) cur_line--;
            int len = strlen(lines[cur_line]);
            if (cur_col > len) cur_col = len;
            if (cur_line < top_line) top_line = cur_line;
            status_msg[0] = '\0';
        } else if (ch == CHAR_DOWN) {
            if (cur_line < line_count - 1) cur_line++;
            int len = strlen(lines[cur_line]);
            if (cur_col > len) cur_col = len;
            if (cur_line >= top_line + EDIT_ROWS) top_line = cur_line - EDIT_ROWS + 1;
            status_msg[0] = '\0';
        } else if (ch == CHAR_LEFT) {
            if (cur_col > 0) {
                cur_col--;
            } else if (cur_line > 0) {
                cur_line--;
                cur_col = strlen(lines[cur_line]);
                if (cur_line < top_line) top_line = cur_line;
            }
            status_msg[0] = '\0';
        } else if (ch == CHAR_RIGHT) {
            int len = strlen(lines[cur_line]);
            if (cur_col < len) {
                cur_col++;
            } else if (cur_line < line_count - 1) {
                cur_line++;
                cur_col = 0;
                if (cur_line >= top_line + EDIT_ROWS) top_line = cur_line - EDIT_ROWS + 1;
            }
            status_msg[0] = '\0';
        } else if (ch == CHAR_HOME) {
            cur_col = 0;
        } else if (ch == CHAR_END) {
            cur_col = strlen(lines[cur_line]);
        } else if (ch == 27) { // Escape key
            break;
        } else if (ch == '\b' || ch == 127) { // Backspace
            if (cur_col > 0) {
                int len = strlen(lines[cur_line]);
                for (int i = cur_col - 1; i < len; i++) {
                    lines[cur_line][i] = lines[cur_line][i + 1];
                }
                cur_col--;
                modified = 1;
            } else if (cur_line > 0) {
                // Merge with previous line
                int prev_len = strlen(lines[cur_line - 1]);
                int cur_len = strlen(lines[cur_line]);
                if (prev_len + cur_len < MAX_LINE_LEN - 1) {
                    strcat(lines[cur_line - 1], lines[cur_line]);
                    for (int i = cur_line; i < line_count - 1; i++) {
                        strcpy(lines[i], lines[i + 1]);
                    }
                    line_count--;
                    cur_line--;
                    cur_col = prev_len;
                    if (cur_line < top_line) top_line = cur_line;
                    modified = 1;
                }
            }
            status_msg[0] = '\0';
        } else if (ch == '\n' || ch == '\r') { // Enter
            if (line_count < MAX_LINES - 1) {
                // Split line
                for (int i = line_count; i > cur_line + 1; i--) {
                    strcpy(lines[i], lines[i - 1]);
                }
                strcpy(lines[cur_line + 1], &lines[cur_line][cur_col]);
                lines[cur_line][cur_col] = '\0';
                line_count++;
                cur_line++;
                cur_col = 0;
                if (cur_line >= top_line + EDIT_ROWS) top_line = cur_line - EDIT_ROWS + 1;
                modified = 1;
            }
            status_msg[0] = '\0';
        } else if ((unsigned char)ch >= 32 && (unsigned char)ch <= 126) { // Printable characters
            int len = strlen(lines[cur_line]);
            if (len < MAX_LINE_LEN - 2) {
                for (int i = len; i >= cur_col; i--) {
                    lines[cur_line][i + 1] = lines[cur_line][i];
                }
                lines[cur_line][cur_col] = ch;
                cur_col++;
                modified = 1;
            }
            status_msg[0] = '\0';
        }
    }

    vga_clear();
    return 0;
}
