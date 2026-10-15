// GNU less Terminal Pager for LemonOS
// Author: Dwip (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "less.h"
#include "vga.h"
#include "keyboard.h"
#include "filesystem.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

#define MAX_LESS_LINES 2048
#define MAX_LINE_WIDTH 160
#define VIEW_HEIGHT 24

static char* less_lines[MAX_LESS_LINES];
static int total_lines = 0;
static int cur_top = 0;
static char search_query[64] = "";
static char prompt_msg[80] = "";

static void less_render(const char* filename) {
    for (int r = 0; r < VIEW_HEIGHT; r++) {
        int lidx = cur_top + r;
        vga_set_cursor(0, r);

        if (lidx < total_lines) {
            const char* line = less_lines[lidx];
            int len = strlen(line);
            int has_match = search_query[0] && strstr(line, search_query) != NULL;

            for (int c = 0; c < VGA_WIDTH; c++) {
                char ch = (c < len) ? line[c] : ' ';
                uint8_t fg = has_match ? VGA_COLOR_BLACK : VGA_COLOR_WHITE;
                uint8_t bg = has_match ? VGA_COLOR_YELLOW : VGA_COLOR_BLACK;
                vga_putchar_color(ch, fg, bg);
            }
        } else {
            // Empty buffer line
            vga_putchar_color('~', VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
            for (int c = 1; c < VGA_WIDTH; c++) {
                vga_putchar_color(' ', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            }
        }
    }

    // Bottom prompt line (Row 24)
    vga_set_cursor(0, 24);
    char pbuf[80];
    if (prompt_msg[0]) {
        snprintf(pbuf, sizeof(pbuf), "%s", prompt_msg);
    } else {
        int pct = (total_lines > 0) ? ((cur_top + VIEW_HEIGHT) * 100 / total_lines) : 100;
        if (pct > 100) pct = 100;
        snprintf(pbuf, sizeof(pbuf), ": %s (lines %d-%d/%d %d%%)", 
                 filename ? filename : "stdin",
                 cur_top + 1, 
                 (cur_top + VIEW_HEIGHT > total_lines) ? total_lines : cur_top + VIEW_HEIGHT,
                 total_lines, pct);
    }
    vga_print_color(pbuf, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    for (int c = strlen(pbuf); c < VGA_WIDTH; c++) {
        vga_putchar_color(' ', VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    }
}

int less_main(int argc, char** argv) {
    if (argc < 2 || !argv[1]) {
        vga_println("Usage: less <filename>");
        return 1;
    }

    const char* filename = argv[1];
    char* content = fs_read_file(filename);
    if (!content) {
        printf("less: %s: No such file or directory\n", filename);
        return 1;
    }

    // Split into lines
    total_lines = 0;
    cur_top = 0;
    search_query[0] = '\0';
    prompt_msg[0] = '\0';

    char* p = content;
    while (*p && total_lines < MAX_LESS_LINES) {
        char* start = p;
        while (*p && *p != '\n' && *p != '\r') p++;
        int len = p - start;
        char* line_buf = (char*)malloc(len + 1);
        if (line_buf) {
            strncpy(line_buf, start, len);
            line_buf[len] = '\0';
            less_lines[total_lines++] = line_buf;
        }
        if (*p == '\r') p++;
        if (*p == '\n') p++;
    }

    if (total_lines == 0) {
        less_lines[0] = "";
        total_lines = 1;
    }

    vga_clear();

    while (1) {
        less_render(filename);

        poll_keyboard();
        while (!has_key()) poll_keyboard();
        char ch = getchar();

        prompt_msg[0] = '\0';

        if (ch == 'q' || ch == 'Q' || ch == 27) { // Quit
            break;
        } else if (ch == ' ' || ch == 'f' || ch == 'F') { // Page down
            cur_top += VIEW_HEIGHT;
            if (cur_top >= total_lines) cur_top = total_lines - 1;
        } else if (ch == 'b' || ch == 'B') { // Page up
            cur_top -= VIEW_HEIGHT;
            if (cur_top < 0) cur_top = 0;
        } else if (ch == 'j' || ch == CHAR_DOWN || ch == '\n' || ch == '\r') { // Line down
            if (cur_top < total_lines - 1) cur_top++;
        } else if (ch == 'k' || ch == CHAR_UP) { // Line up
            if (cur_top > 0) cur_top--;
        } else if (ch == 'g' || ch == CHAR_HOME) { // Top
            cur_top = 0;
        } else if (ch == 'G' || ch == CHAR_END) { // Bottom
            cur_top = (total_lines > VIEW_HEIGHT) ? (total_lines - VIEW_HEIGHT) : 0;
        } else if (ch == '/') { // Search mode
            vga_set_cursor(0, 24);
            vga_print_color("/", VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
            for (int c = 1; c < VGA_WIDTH; c++) vga_putchar_color(' ', VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
            vga_set_cursor(1, 24);

            int qlen = 0;
            search_query[0] = '\0';
            while (1) {
                poll_keyboard();
                while (!has_key()) poll_keyboard();
                char sch = getchar();
                if (sch == '\n' || sch == '\r' || sch == 27) break;
                if ((sch == '\b' || sch == 127) && qlen > 0) {
                    qlen--;
                    search_query[qlen] = '\0';
                    vga_putchar_at(1 + qlen, 24, ' ');
                    vga_set_cursor(1 + qlen, 24);
                } else if (qlen < (int)sizeof(search_query) - 2 && (unsigned char)sch >= 32 && (unsigned char)sch <= 126) {
                    search_query[qlen++] = sch;
                    search_query[qlen] = '\0';
                    vga_putchar_at(qlen, 24, sch);
                    vga_set_cursor(1 + qlen, 24);
                }
            }
            // Jump to first match
            if (search_query[0]) {
                int found = 0;
                for (int i = cur_top; i < total_lines; i++) {
                    if (strstr(less_lines[i], search_query)) {
                        cur_top = i;
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    for (int i = 0; i < cur_top; i++) {
                        if (strstr(less_lines[i], search_query)) {
                            cur_top = i;
                            found = 1;
                            break;
                        }
                    }
                }
                if (!found) {
                    snprintf(prompt_msg, sizeof(prompt_msg), "Pattern not found: %s", search_query);
                }
            }
        } else if (ch == 'n' && search_query[0]) { // Next match
            for (int i = cur_top + 1; i < total_lines; i++) {
                if (strstr(less_lines[i], search_query)) {
                    cur_top = i;
                    break;
                }
            }
        }
    }

    // Clean up allocated line buffers
    for (int i = 0; i < total_lines; i++) {
        if (less_lines[i] && less_lines[i][0]) free(less_lines[i]);
    }

    vga_clear();
    return 0;
}
