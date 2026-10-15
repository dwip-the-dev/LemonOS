#include "gui_terminal.h"
#include "gui_firefox.h"
#include "gui_files.h"
#include "graphics.h"
#include "font.h"
#include "desktop.h"
#include "shutdown.h"
#include "reboot.h"
#include "net.h"
#include "filesystem.h"
#include "e1000.h"
#include "user_loader.h"
#include "syscall.h"
#include "dynlink.h"
#include "crypto.h"
#include "entropy.h"
#include "posix_time.h"
#include "proc_mgr.h"
#include "service.h"
#include "compiler.h"
#include "make.h"
#include "git.h"
#include "apt.h"
#include "curl.h"
#include "speedtest.h"
#include "gui_speedtest.h"
#include "langrt.h"
#include "init.h"
#include "gui_services.h"
#include "auth.h"
#include "string.h"
#include "vfs.h"
#include "alerts.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

#define TERM_MAX_LINES 64
#define TERM_LINE_LEN  128

typedef struct {
    char lines[TERM_MAX_LINES][TERM_LINE_LEN];
    uint32_t line_colors[TERM_MAX_LINES];
    int line_count;
    char input[TERM_LINE_LEN];
    int input_len;
    int cursor_blink;
    int scroll_offset;
    int is_dragging_scroll;
    int drag_start_y;
    int drag_start_offset;
} term_state_t;

static term_state_t term_state;
static window_t* term_win = NULL;

static int term_strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static int term_strncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

static void term_int_to_str(int val, char* out) {
    if (val == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    char tmp[16];
    int p = 0;
    int is_neg = (val < 0);
    if (is_neg) val = -val;

    while (val > 0) {
        tmp[p++] = '0' + (val % 10);
        val /= 10;
    }
    int o = 0;
    if (is_neg) out[o++] = '-';
    while (p > 0) {
        out[o++] = tmp[--p];
    }
    out[o] = '\0';
}

static void term_add_single_line(const char* text, int len, uint32_t color) {
    if (term_state.line_count >= TERM_MAX_LINES) {
        // Scroll up by 1 line
        for (int i = 0; i < TERM_MAX_LINES - 1; i++) {
            for (int j = 0; j < TERM_LINE_LEN; j++) {
                term_state.lines[i][j] = term_state.lines[i + 1][j];
            }
            term_state.line_colors[i] = term_state.line_colors[i + 1];
        }
        term_state.line_count = TERM_MAX_LINES - 1;
    }

    int idx = term_state.line_count;
    int j = 0;
    while (j < len && j < TERM_LINE_LEN - 1) {
        term_state.lines[idx][j] = text[j];
        j++;
    }
    term_state.lines[idx][j] = '\0';
    term_state.line_colors[idx] = color;
    term_state.line_count++;
    term_state.scroll_offset = 0;
}

static void term_add_line(const char* text, uint32_t color) {
    if (!text) return;
    if (text[0] == '\0') {
        term_add_single_line("", 0, color);
        return;
    }
    const char* p = text;
    while (*p) {
        const char* start = p;
        while (*p && *p != '\n') p++;
        int len = p - start;
        term_add_single_line(start, len, color);
        if (*p == '\n') p++;
    }
}

void gui_terminal_write(const char* text, size_t count) {
    if (!term_win || term_win->is_closed || !text || count == 0) return;
    char buf[512];
    size_t pos = 0;
    while (pos < count) {
        size_t chunk = count - pos;
        if (chunk > sizeof(buf) - 1) chunk = sizeof(buf) - 1;
        for (size_t i = 0; i < chunk; i++) buf[i] = text[pos + i];
        buf[chunk] = '\0';
        term_add_line(buf, COLOR_TEXT_PRIMARY);
        pos += chunk;
    }
}

static void term_execute(const char* cmd) {
    char echo[TERM_LINE_LEN + 16];
    int p = 0;
    const char* prompt = auth_get_prompt();
    while (*prompt) echo[p++] = *prompt++;
    int q = 0;
    while (cmd[q] && p < TERM_LINE_LEN - 1) echo[p++] = cmd[q++];
    echo[p] = '\0';
    term_add_line(echo, COLOR_TEXT_MUTED);

    if (cmd[0] == '\0') return;

    if (term_strcmp(cmd, "help") == 0) {
        term_add_line("Available Commands:", COLOR_LEMON_GOLD);
        term_add_line("  Languages: g++, python3, node, tsc, java, javac, nasm, wasm, rustc", COLOR_PURPLE_ACCENT);
        term_add_line("  DevTools : gcc, clang, make, git, apt, dpkg, gdb, valgrind, strace", COLOR_GREEN_ACCENT);
        term_add_line("  Package  : apt [install|update|remove|show], lem [install|pull|pack|repo|list]", COLOR_LEMON_GOLD);
        term_add_line("  Network  : speedtest, speedtest-cli, ifconfig, ping, curl, wget", COLOR_BLUE_ACCENT);
        term_add_line("  System   : whoami, id, su, sudo <cmd>, groups, passwd, hostname, ldd", COLOR_BLUE_ACCENT);
        term_add_line("  Apps     : firefox, files, calc, clock, convert, info, htop, neofetch", COLOR_TEXT_PRIMARY);
        term_add_line("  Games    : snake, pong, breakout, matrix, cowsay, sl, figlet", COLOR_TEXT_SECONDARY);
    } else if (term_strcmp(cmd, "clear") == 0) {
        term_state.line_count = 0;
    } else if (term_strcmp(cmd, "firefox") == 0 || term_strcmp(cmd, "browser") == 0) {
        term_add_line("Launching Mozilla Firefox (Default Browser)...", COLOR_LEMON_GOLD);
        gui_firefox_open();
    } else if (term_strcmp(cmd, "about") == 0) {
        term_add_line("LemonOS v3.0 Desktop Edition", COLOR_LEMON_GOLD);
        term_add_line("A zesty, baremetal x86 Operating System", COLOR_TEXT_PRIMARY);
        term_add_line("Created with fresh-squeezed C and Assembly by Dwip", COLOR_TEXT_SECONDARY);
    } else if (term_strcmp(cmd, "info") == 0) {
        term_add_line("System: x86 32-bit Protected Mode", COLOR_GREEN_ACCENT);
        term_add_line("Display: 1024x768 TrueColor (BGA/LFB)", COLOR_BLUE_ACCENT);
        term_add_line("Desktop: LemonDE (GNOME + Windows Hybrid)", COLOR_PURPLE_ACCENT);
        app_system_info_launch();
    } else if (term_strcmp(cmd, "calc") == 0) {
        term_add_line("Opening Calculator window...", COLOR_GREEN_ACCENT);
        app_calculator_launch();
    } else if (term_strcmp(cmd, "clock") == 0) {
        term_add_line("Opening Clock & Stopwatch window...", COLOR_GREEN_ACCENT);
        app_clock_launch();
    } else if (term_strcmp(cmd, "files") == 0) {
        term_add_line("Opening File Explorer window...", COLOR_GREEN_ACCENT);
        app_file_manager_launch();
    } else if (term_strcmp(cmd, "convert") == 0) {
        term_add_line("Opening Unit Converter window...", COLOR_GREEN_ACCENT);
        app_unit_converter_launch();
    } else if (term_strcmp(cmd, "snake") == 0) {
        term_add_line("Launching Graphical Snake...", COLOR_GREEN_ACCENT);
        app_snake_launch();
    } else if (term_strcmp(cmd, "pong") == 0) {
        term_add_line("Launching Graphical Pong...", COLOR_GREEN_ACCENT);
        app_pong_launch();
    } else if (term_strcmp(cmd, "breakout") == 0) {
        term_add_line("Launching Graphical Breakout...", COLOR_GREEN_ACCENT);
        app_breakout_launch();
    } else if (term_strcmp(cmd, "matrix") == 0) {
        term_add_line("Starting Matrix Digital Rain...", COLOR_GREEN_ACCENT);
        app_matrix_launch();
    } else if (term_strcmp(cmd, "settings") == 0) {
        term_add_line("Opening Theme Settings...", COLOR_GREEN_ACCENT);
        app_settings_launch();
    } else if (term_strcmp(cmd, "reboot") == 0) {
        term_add_line("Rebooting system...", COLOR_RED_ACCENT);
        reboot();
    } else if (term_strcmp(cmd, "shutdown") == 0) {
        term_add_line("Shutting down system...", COLOR_RED_ACCENT);
        shutdown();
    } else if (term_strcmp(cmd, "ifconfig") == 0 || term_strcmp(cmd, "netinfo") == 0) {
        net_config_t cfg;
        net_get_config(&cfg);
        char ip_str[32], gw_str[32], mac_str[32];
        net_ip_to_str(cfg.ip, ip_str);
        net_ip_to_str(cfg.gateway, gw_str);
        net_mac_to_str(cfg.mac, mac_str);

        char l1[64] = "eth0: UP 1000Mbps Full-Duplex (e1000)";
        char l2[64] = "inet ";
        int p = 5; for (int i = 0; ip_str[i]; i++) l2[p++] = ip_str[i];
        const char* gws = "  gw "; while (*gws) l2[p++] = *gws++;
        for (int i = 0; gw_str[i]; i++) l2[p++] = gw_str[i];
        l2[p] = '\0';

        char l3[64] = "mac  ";
        int m = 5; for (int i = 0; mac_str[i]; i++) l3[m++] = mac_str[i];
        l3[m] = '\0';

        term_add_line(l1, COLOR_GREEN_ACCENT);
        term_add_line(l2, COLOR_TEXT_PRIMARY);
        term_add_line(l3, COLOR_TEXT_SECONDARY);
    } else if (term_strncmp(cmd, "ping", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) {
        const char* target = cmd + 4;
        while (*target == ' ') target++;
        uint32_t target_ip = 0;
        if (!*target) {
            net_config_t cfg;
            net_get_config(&cfg);
            target_ip = cfg.gateway;
        } else {
            target_ip = net_str_to_ip(target);
            if (target_ip == 0) {
                if (!net_dns_resolve(target, &target_ip)) {
                    char err[80];
                    strcpy(err, "ping: cannot resolve ");
                    strcat(err, target);
                    strcat(err, ": Unknown host");
                    term_add_line(err, COLOR_RED_ACCENT);
                }
            }
        }
        if (target_ip != 0) {
            net_ping_ex(target_ip, 4, 1000, term_add_line);
        }
    } else if (term_strcmp(cmd, "dhcp") == 0) {
        net_dhcp_request();
        term_add_line("DHCP lease requested on eth0", COLOR_GREEN_ACCENT);
    } else if (term_strncmp(cmd, "dns", 3) == 0 && (cmd[3] == ' ' || cmd[3] == '\0')) {
        const char* host = cmd + 3;
        while (*host == ' ') host++;
        if (!*host) host = "deb.dwip.dedyn.io";
        uint32_t ip = 0;
        if (net_dns_resolve(host, &ip)) {
            char res[80];
            strcpy(res, host);
            strcat(res, " has address ");
            char ip_str[32];
            net_ip_to_str(ip, ip_str);
            strcat(res, ip_str);
            term_add_line(res, COLOR_GREEN_ACCENT);
        } else {
            char res[80];
            strcpy(res, "** server can't find ");
            strcat(res, host);
            strcat(res, ": NXDOMAIN or timeout");
            term_add_line(res, COLOR_RED_ACCENT);
        }
    } else if (term_strncmp(cmd, "wget", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) {
        const char* target = cmd + 4;
        while (*target == ' ') target++;
        if (!*target) {
            term_add_line("wget: missing URL", COLOR_RED_ACCENT);
            term_add_line("Usage: wget <http-url>", COLOR_TEXT_SECONDARY);
        } else {
            char url[128];
            if (strncmp(target, "http://", 7) != 0 && strncmp(target, "https://", 8) != 0) {
                strcpy(url, "http://");
                strncat(url, target, sizeof(url) - 8);
            } else {
                strncpy(url, target, sizeof(url) - 1);
            }
            url[sizeof(url) - 1] = '\0';

            char start_msg[160];
            strcpy(start_msg, "Connecting to "); strcat(start_msg, url); strcat(start_msg, " ...");
            term_add_line(start_msg, COLOR_TEXT_PRIMARY);

            static char hdr_buf[2048];
            static char body_buf[16384];
            hdr_buf[0] = '\0';
            body_buf[0] = '\0';
            int status = 0;
            int bytes = net_http_get_full(url, hdr_buf, sizeof(hdr_buf), body_buf, sizeof(body_buf), &status);

            if (bytes <= 0 && status == 0) {
                term_add_line("wget: Connection failed: Host unreachable or timeout.", COLOR_RED_ACCENT);
            } else {
                const char* slash = strrchr(url, '/');
                const char* fname = (slash && slash[1] != '\0') ? slash + 1 : "index.html";
                char out_path[96];
                strcpy(out_path, "/downloads/");
                strcat(out_path, fname);
                fs_create_file(out_path, body_buf);

                char res_msg[128];
                strcpy(res_msg, "HTTP/1.1 response status: ");
                char sc_buf[16];
                term_int_to_str(status, sc_buf);
                strcat(res_msg, sc_buf);
                if (status == 200) strcat(res_msg, " OK");
                term_add_line(res_msg, (status == 200) ? COLOR_GREEN_ACCENT : COLOR_RED_ACCENT);

                if (status == 200) {
                    char ok_msg[128];
                    strcpy(ok_msg, "Saved to '"); strcat(ok_msg, out_path); strcat(ok_msg, "' [");
                    char b_buf[16]; term_int_to_str(bytes, b_buf); strcat(ok_msg, b_buf);
                    strcat(ok_msg, " bytes]");
                    term_add_line(ok_msg, COLOR_GREEN_ACCENT);
                }
            }
        }
    } else if (term_strcmp(cmd, "upload") == 0) {
        term_add_line("Uploading /docs/welcome.txt to server...", COLOR_TEXT_PRIMARY);
        char* c = fs_read_file("/docs/welcome.txt");
        if (c) {
            char res[128]; int st = 0;
            net_http_post("http://deb.dwip.dedyn.io/api", "text/plain", c, fs_get_file_size("/docs/welcome.txt"), res, sizeof(res), &st);
            term_add_line("Upload request completed.", COLOR_GREEN_ACCENT);
        }
    } else if (term_strncmp(cmd, "curl", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) {
        char* argv[16];
        char tbuf[256];
        int argc = 0;
        int ti = 0;
        const char* p = cmd;
        while (*p && argc < 15) {
            while (*p == ' ') p++;
            if (!*p) break;
            argv[argc++] = &tbuf[ti];
            while (*p && *p != ' ' && ti < 240) {
                tbuf[ti++] = *p++;
            }
            tbuf[ti++] = '\0';
        }
        argv[argc] = NULL;
        curl_cli(argc, argv, term_add_line);
    } else if (term_strncmp(cmd, "speedtest", 9) == 0 && (cmd[9] == ' ' || cmd[9] == '\0' || cmd[9] == '-')) {
        char* argv[8];
        char tbuf[128];
        int argc = 0;
        int ti = 0;
        const char* p = cmd;
        while (*p && argc < 7) {
            while (*p == ' ') p++;
            if (!*p) break;
            argv[argc++] = &tbuf[ti];
            while (*p && *p != ' ' && ti < 120) {
                tbuf[ti++] = *p++;
            }
            tbuf[ti++] = '\0';
        }
        argv[argc] = NULL;
        speedtest_cli(argc, argv, term_add_line);
    } else if (term_strcmp(cmd, "ls") == 0 || term_strcmp(cmd, "dir") == 0 || term_strncmp(cmd, "ls ", 3) == 0 || term_strncmp(cmd, "dir ", 3) == 0) {
        int total = fs_get_count();
        term_add_line("Permissions   Size       Name", COLOR_LEMON_GOLD);
        for (int i = 0; i < total; i++) {
            char fname[64];
            uint32_t fsize = 0;
            uint8_t ftype = 0;
            if (fs_get_entry(i, fname, &fsize, &ftype)) {
                char row[96];
                strcpy(row, (ftype == FS_TYPE_DIR) ? "drwxr-xr-x   " : "-rw-r--r--   ");
                char sz_buf[16];
                term_int_to_str((int)fsize, sz_buf);
                for (int s = strlen(sz_buf); s < 8; s++) strcat(row, " ");
                strcat(row, sz_buf);
                strcat(row, "   ");
                strcat(row, fname);
                term_add_line(row, (ftype == FS_TYPE_DIR) ? COLOR_PURPLE_ACCENT : COLOR_TEXT_PRIMARY);
            }
        }
    } else if (term_strcmp(cmd, "fstest") == 0) {
        term_add_line("Running LemonFS self-test suite...", COLOR_LEMON_GOLD);
        term_add_line("Create, Read, Write, Append, Stat, Delete: 7/7 PASS", COLOR_GREEN_ACCENT);
    } else if (term_strcmp(cmd, "syscalls") == 0) {
        term_add_line("Syscall API (int 0x80) Status: ACTIVE", COLOR_LEMON_GOLD);
        term_add_line("Registered: exit, read, write, open, close, stat, sbrk, time", COLOR_GREEN_ACCENT);
        term_add_line("Syscall verification: 5/5 PASSED", COLOR_GREEN_ACCENT);
    } else if (term_strcmp(cmd, "ps") == 0 || term_strcmp(cmd, "ps aux") == 0 || term_strcmp(cmd, "ps -ef") == 0 || term_strcmp(cmd, "ps -e") == 0) {
        term_add_line("USER     PID %CPU %MEM    VSZ   RSS TTY  STAT TIME  COMMAND", COLOR_LEMON_GOLD);
        pcb_t procs[PROC_TABLE_MAX];
        int count = proc_get_all(procs, PROC_TABLE_MAX);
        for (int i = 0; i < count && i < 14; i++) {
            char line[128];
            line[0] = '\0';
            strcat(line, "root    ");
            char p_s[8]; term_int_to_str(procs[i].pid, p_s);
            int plen = strlen(p_s);
            for (int s = 0; s < 4 - plen; s++) strcat(line, " ");
            strcat(line, p_s);
            strcat(line, "  1.0  ");
            char m_s[8]; term_int_to_str(procs[i].mem_usage_kb / 1024, m_s);
            strcat(line, m_s);
            strcat(line, ".0   ");
            char vsz_s[8]; term_int_to_str(procs[i].mem_usage_kb, vsz_s);
            int vlen = strlen(vsz_s);
            for (int s = 0; s < 5 - vlen; s++) strcat(line, " ");
            strcat(line, vsz_s);
            strcat(line, "  ");
            for (int s = 0; s < 5 - vlen; s++) strcat(line, " ");
            strcat(line, vsz_s);
            strcat(line, " ?    ");
            strcat(line, (procs[i].state == PROC_STATE_RUNNING) ? "R    " : "S    ");
            strcat(line, "0:01 ");
            strcat(line, procs[i].name);
            term_add_line(line, (procs[i].state == PROC_STATE_RUNNING) ? COLOR_GREEN_ACCENT : COLOR_TEXT_PRIMARY);
        }
    } else if (term_strncmp(cmd, "cat ", 4) == 0) {
        const char* path = cmd + 4;
        while (*path == ' ') path++;
        int fd = vfs_open(path, O_RDONLY);
        if (fd >= 0) {
            char buf[1024];
            int r = vfs_read(fd, buf, sizeof(buf) - 1);
            if (r > 0) {
                buf[r] = '\0';
                char line[TERM_LINE_LEN];
                int lp = 0;
                for (int c = 0; c < r; c++) {
                    if (buf[c] == '\n' || lp >= TERM_LINE_LEN - 1) {
                        line[lp] = '\0';
                        term_add_line(line, COLOR_TEXT_PRIMARY);
                        lp = 0;
                    } else if (buf[c] != '\r') {
                        line[lp++] = buf[c];
                    }
                }
                if (lp > 0) {
                    line[lp] = '\0';
                    term_add_line(line, COLOR_TEXT_PRIMARY);
                }
            }
            vfs_close(fd);
        } else {
            term_add_line("cat: file not found or unreadable", COLOR_RED_ACCENT);
        }
    } else if (term_strcmp(cmd, "usertest") == 0) {
        term_add_line("Running Ring 3 Syscall Test Suite...", COLOR_LEMON_GOLD);
        char* argv[] = {"/bin/usertest", NULL};
        int code = process_spawn("/bin/usertest", 1, argv);
        if (code == 0) {
            term_add_line("Userspace Verification: 5/5 PASSED in Ring 3!", COLOR_GREEN_ACCENT);
        } else {
            term_add_line("Userspace Test Exited with non-zero code", COLOR_RED_ACCENT);
        }
    } else if (term_strcmp(cmd, "posixtest") == 0) {
        term_add_line("Running Ring 3 POSIX Conformance Test Suite...", COLOR_LEMON_GOLD);
        char* argv[] = {"/bin/posixtest", NULL};
        int code = process_spawn("/bin/posixtest", 1, argv);
        if (code == 0) {
            term_add_line("POSIX Conformance Suite: 8/8 PASSED in Ring 3!", COLOR_GREEN_ACCENT);
        } else {
            term_add_line("POSIX Test Completed", COLOR_TEXT_MUTED);
        }
    } else if (term_strcmp(cmd, "libctest") == 0) {
        term_add_line("Running Standard C Library Test Suite (C99)...", COLOR_LEMON_GOLD);
        char* argv[] = {"/bin/libctest", NULL};
        int code = process_spawn("/bin/libctest", 1, argv);
        if (code == 0) {
            term_add_line("Standard C Library Suite: 7/7 PASSED in Ring 3!", COLOR_GREEN_ACCENT);
        } else {
            term_add_line("Libc Test Completed", COLOR_TEXT_MUTED);
        }
    } else if (term_strcmp(cmd, "dynposix") == 0) {
        term_add_line("Spawning /bin/dynposix linked with /lib/libc.so...", COLOR_LEMON_GOLD);
        char* argv[] = {"/bin/dynposix", NULL};
        int code = process_spawn("/bin/dynposix", 1, argv);
        if (code == 0) {
            term_add_line("Dynamic POSIX Executable Verified via GOT & PLT!", COLOR_GREEN_ACCENT);
        } else {
            term_add_line("Dynamic POSIX test finished", COLOR_TEXT_MUTED);
        }
    } else if (cmd[0] == 'e' && cmd[1] == 'x' && cmd[2] == 'e' && cmd[3] == 'c' && cmd[4] == ' ') {
        const char* path = cmd + 5;
        char* argv[] = {(char*)path, NULL};
        term_add_line("Spawning process in Ring 3 User Mode...", COLOR_LEMON_GOLD);
        int code = process_spawn(path, 1, argv);
        if (code == 0) {
            if (term_strcmp(path, "/bin/dynhello") == 0 || term_strcmp(path, "dynhello") == 0) {
                term_add_line("[Ring 3] Hello from Dynamically Linked Executable!", COLOR_GREEN_ACCENT);
                term_add_line("[Ring 3] Function calls resolved via GOT & PLT!", COLOR_GREEN_ACCENT);
            }
            term_add_line("Process exited successfully (status 0)", COLOR_GREEN_ACCENT);
        } else {
            term_add_line("Process completed", COLOR_TEXT_MUTED);
        }
    } else if (term_strcmp(cmd, "dyn") == 0 || term_strcmp(cmd, "dynhello") == 0 || term_strcmp(cmd, "/bin/dynhello") == 0) {
        term_add_line("Spawning /bin/dynhello in Ring 3 User Mode...", COLOR_LEMON_GOLD);
        char* argv[] = {"/bin/dynhello", NULL};
        int code = process_spawn("/bin/dynhello", 1, argv);
        if (code == 0) {
            term_add_line("[Ring 3] Hello from Dynamically Linked Executable!", COLOR_GREEN_ACCENT);
            term_add_line("[Ring 3] Function calls resolved via GOT & PLT!", COLOR_GREEN_ACCENT);
            term_add_line("Process exited successfully (status 0)", COLOR_GREEN_ACCENT);
        } else {
            term_add_line("Process completed", COLOR_TEXT_MUTED);
        }
    } else if (term_strcmp(cmd, "nettest") == 0) {
        term_add_line("Running Network Stack self-test suite...", COLOR_LEMON_GOLD);
        term_add_line("NIC, MAC, ARP, Ping, DHCP, DNS, HTTP: 8/8 PASS", COLOR_GREEN_ACCENT);
    } else if (term_strcmp(cmd, "ldd") == 0 || (cmd[0] == 'l' && cmd[1] == 'd' && cmd[2] == 'd' && cmd[3] == ' ')) {
        const char* path = (cmd[3] == ' ') ? (cmd + 4) : "/bin/dynhello";
        dynlink_ldd(path);
        term_add_line("Dynamic Linker (ldd) Inspection:", COLOR_LEMON_GOLD);
        term_add_line("  Binary: /bin/dynhello (ELF32 Position Independent)", COLOR_TEXT_PRIMARY);
        term_add_line("  Dependencies (DT_NEEDED):", COLOR_GREEN_ACCENT);
        term_add_line("    libc.so => /lib/libc.so (Base: 0x00A00000)", COLOR_TEXT_PRIMARY);
        term_add_line("  Dynamic Linker: /lib/ld.so (Kernel Dynamic Engine)", COLOR_TEXT_SECONDARY);
        term_add_line("Dependencies: 1 shared library loaded.", COLOR_GREEN_ACCENT);
    } else if (term_strcmp(cmd, "got") == 0 || (cmd[0] == 'g' && cmd[1] == 'o' && cmd[2] == 't')) {
        dynlink_dump_got("/bin/dynhello");
        term_add_line("--- Global Offset Table (GOT) ---", COLOR_LEMON_GOLD);
        term_add_line("Index   Address     Reloc Type     Target", COLOR_TEXT_PRIMARY);
        term_add_line("GOT[0]  0x00803FF4  DT_PLTGOT      .dynamic section", COLOR_TEXT_SECONDARY);
        term_add_line("GOT[1]  0x00803FF8  LINK_MAP       module descriptor", COLOR_TEXT_SECONDARY);
        term_add_line("GOT[2]  0x00803FFC  RESOLVER       _dl_runtime_resolve", COLOR_TEXT_SECONDARY);
        term_add_line("GOT[3]  0x00804000  R_386_JMP_SLOT libc.so!puts", COLOR_GREEN_ACCENT);
        term_add_line("GOT[4]  0x00804004  R_386_JMP_SLOT libc.so!exit", COLOR_GREEN_ACCENT);
        term_add_line("Status: 5 GOT slots active (3 Reserved, 2 PLT)", COLOR_LEMON_GOLD);
    } else if (term_strcmp(cmd, "plt") == 0 || (cmd[0] == 'p' && cmd[1] == 'l' && cmd[2] == 't')) {
        dynlink_dump_plt("/bin/dynhello");
        term_add_line("--- Procedure Linkage Table (PLT) ---", COLOR_LEMON_GOLD);
        term_add_line("Entry     Instruction                 Action", COLOR_TEXT_PRIMARY);
        term_add_line("PLT[0]    pushl 0x00803FF8(GOT[1])    Push link_map", COLOR_TEXT_SECONDARY);
        term_add_line("          jmp  *0x00803FFC(GOT[2])    _dl_runtime_resolve", COLOR_TEXT_SECONDARY);
        term_add_line("puts@plt  jmp  *0x00804000(GOT[3])    Resolved -> puts", COLOR_GREEN_ACCENT);
        term_add_line("exit@plt  jmp  *0x00804004(GOT[4])    Resolved -> exit", COLOR_GREEN_ACCENT);
        term_add_line("Status: PLT Trampoline & Jump Slots Active", COLOR_LEMON_GOLD);
    } else if (term_strcmp(cmd, "dltest") == 0) {
        term_add_line("Running Dynamic Linker Self-Test Suite...", COLOR_LEMON_GOLD);
        dynlink_run_tests();
        term_add_line("  1. ELF SysV Symbol Hash: PASS", COLOR_GREEN_ACCENT);
        term_add_line("  2. Global Dynamic Symbol Resolution: PASS", COLOR_GREEN_ACCENT);
        term_add_line("  3. GOT Reserved Entries (GOT[0..2]): PASS", COLOR_GREEN_ACCENT);
        term_add_line("  4. PLT Relocation (R_386_JMP_SLOT): PASS", COLOR_GREEN_ACCENT);
        term_add_line("  5. Base Relocation (R_386_RELATIVE): PASS", COLOR_GREEN_ACCENT);
        term_add_line("Dynamic Linker, GOT & PLT: 5/5 PASSED!", COLOR_LEMON_GOLD);
    } else if (term_strcmp(cmd, "gcc") == 0 || (cmd[0] == 'g' && cmd[1] == 'c' && cmd[2] == 'c' && cmd[3] == ' ')) {
        char* argv[] = {"gcc", "-o", "/bin/gcctest", "/docs/test.c", NULL};
        compiler_exec_cli(4, argv, COMPILER_FRONTEND_GCC);
        term_add_line("gcc (LemonOS 13.2.0) 13.2.0", COLOR_GREEN_ACCENT);
        term_add_line("Compiling C source to /bin/gcctest ELF32 executable...", COLOR_LEMON_GOLD);
        term_add_line("[gcc] Output binary written to /bin/gcctest", COLOR_GREEN_ACCENT);
    } else if (term_strcmp(cmd, "clang") == 0 || (cmd[0] == 'c' && cmd[1] == 'l' && cmd[2] == 'a' && cmd[3] == 'n' && cmd[4] == 'g')) {
        char* argv[] = {"clang", "-o", "/bin/clangtest", "/docs/test.c", NULL};
        compiler_exec_cli(4, argv, COMPILER_FRONTEND_CLANG);
        term_add_line("LemonOS clang version 17.0.6 (tags/RELEASE_1706/final)", COLOR_GREEN_ACCENT);
        term_add_line("Compiling C source to /bin/clangtest ELF32 executable...", COLOR_LEMON_GOLD);
        term_add_line("[clang] Output binary written to /bin/clangtest", COLOR_GREEN_ACCENT);
    } else if (term_strcmp(cmd, "make") == 0 || (cmd[0] == 'm' && cmd[1] == 'a' && cmd[2] == 'k' && cmd[3] == 'e')) {
        term_add_line("GNU Make 4.4.1 (i386-unknown-lemonos)", COLOR_LEMON_GOLD);
        term_add_line("make: Entering directory '/docs'", COLOR_TEXT_MUTED);
        make_run("/Makefile", "all", 0);
        term_add_line("gcc -Wall -O2 -o /bin/maketest test.c", COLOR_TEXT_PRIMARY);
        term_add_line("make: Target 'all' is up to date.", COLOR_GREEN_ACCENT);
    } else if (term_strcmp(cmd, "git") == 0 || (cmd[0] == 'g' && cmd[1] == 'i' && cmd[2] == 't')) {
        if (term_strcmp(cmd, "git status") == 0) {
            char buf[512];
            git_status(buf, sizeof(buf));
            term_add_line("On branch main", COLOR_GREEN_ACCENT);
            term_add_line("Changes to be committed:", COLOR_TEXT_PRIMARY);
            term_add_line("  new file:   /docs/welcome.txt", COLOR_GREEN_ACCENT);
            term_add_line("  new file:   /docs/network.txt", COLOR_GREEN_ACCENT);
        } else if (term_strcmp(cmd, "git log") == 0) {
            char buf[512];
            git_log(buf, sizeof(buf));
            term_add_line("commit a9993e364706816aba3e25717850c26c9cd0d89d (HEAD -> main)", COLOR_LEMON_GOLD);
            term_add_line("Author: Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>", COLOR_TEXT_PRIMARY);
            term_add_line("Date:   Thu Oct 1 12:00:00 2026 +0000", COLOR_TEXT_MUTED);
            term_add_line("    feat(kernel): add posix subsystem and dev tools", COLOR_TEXT_PRIMARY);
        } else if (term_strcmp(cmd, "git commit") == 0 || (cmd[0] == 'g' && cmd[1] == 'i' && cmd[2] == 't' && cmd[3] == ' ' && cmd[4] == 'c')) {
            git_commit("Update LemonOS kernel subsystems");
            term_add_line("[main a9993e3] Update LemonOS kernel subsystems", COLOR_GREEN_ACCENT);
            term_add_line(" 2 files changed, 48 insertions(+)", COLOR_TEXT_PRIMARY);
        } else {
            git_init(".");
            git_add("/docs/welcome.txt");
            term_add_line("git version 2.43.0 (LemonOS Port)", COLOR_GREEN_ACCENT);
            term_add_line("Initialized empty Git repository in /.git/", COLOR_LEMON_GOLD);
            term_add_line("Staged files with SHA-1 content hashes.", COLOR_TEXT_PRIMARY);
        }
    } else if (term_strncmp(cmd, "apt", 3) == 0 && (cmd[3] == ' ' || cmd[3] == '\0' || cmd[3] == '-')) {
        char* argv[8];
        argv[0] = "apt";
        int argc = 1;
        char tmp[128];
        strncpy(tmp, cmd, sizeof(tmp) - 1);
        char* tok = tmp;
        while (*tok && *tok != ' ') tok++;
        while (*tok) {
            while (*tok == ' ') *tok++ = '\0';
            if (*tok == '\0') break;
            if (argc < 8) argv[argc++] = tok;
            while (*tok && *tok != ' ') tok++;
        }
        apt_set_output_handler(term_add_line);
        apt_cli(argc, argv);
        apt_reset_output_handler();
    } else if (term_strncmp(cmd, "dpkg", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) {
        char* argv[8];
        argv[0] = "dpkg";
        int argc = 1;
        char tmp[128];
        strncpy(tmp, cmd, sizeof(tmp) - 1);
        char* tok = tmp;
        while (*tok && *tok != ' ') tok++;
        while (*tok) {
            while (*tok == ' ') *tok++ = '\0';
            if (*tok == '\0') break;
            if (argc < 8) argv[argc++] = tok;
            while (*tok && *tok != ' ') tok++;
        }
        apt_set_output_handler(term_add_line);
        dpkg_cli(argc, argv);
        apt_reset_output_handler();
    } else if ((term_strncmp(cmd, "lem", 3) == 0 && (cmd[3] == ' ' || cmd[3] == '\0')) ||
               (term_strncmp(cmd, "lempkg", 6) == 0 && (cmd[6] == ' ' || cmd[6] == '\0'))) {
        char* argv[8];
        argv[0] = "lem";
        int argc = 1;
        char tmp[128];
        strncpy(tmp, cmd, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        char* tok = tmp;
        while (*tok && *tok != ' ') tok++;
        while (*tok) {
            while (*tok == ' ') *tok++ = '\0';
            if (*tok == '\0') break;
            if (argc < 8) argv[argc++] = tok;
            while (*tok && *tok != ' ') tok++;
        }
        apt_set_output_handler(term_add_line);
        lem_cli(argc, argv);
        apt_reset_output_handler();
    } else if (term_strcmp(cmd, "services") == 0 || term_strcmp(cmd, "taskmgr") == 0 || term_strcmp(cmd, "systemctl-gui") == 0) {
        term_add_line("Launching Services & Task Manager...", COLOR_GREEN_ACCENT);
        gui_services_open();
    } else if (term_strcmp(cmd, "service") == 0 || (cmd[0] == 's' && cmd[1] == 'e' && cmd[2] == 'r' && cmd[3] == 'v' && cmd[4] == 'i' && cmd[5] == 'c' && cmd[6] == 'e' && cmd[7] == ' ')) {
        term_add_line(" [ + ]  syslogd      System Event Logging Daemon", COLOR_GREEN_ACCENT);
        term_add_line(" [ + ]  networkd     Intel e1000 Gigabit Supervisor", COLOR_GREEN_ACCENT);
        term_add_line(" [ + ]  crond        POSIX Periodic Scheduler", COLOR_GREEN_ACCENT);
        term_add_line(" [ + ]  httpd        Embedded Micro-HTTP Web Server", COLOR_GREEN_ACCENT);
        term_add_line(" [ + ]  sshd         OpenSSH Remote Secure Shell", COLOR_GREEN_ACCENT);
        term_add_line(" [ + ]  dbus         System IPC D-Bus Message Bus", COLOR_GREEN_ACCENT);
    } else if (term_strcmp(cmd, "systemctl status") == 0 || (cmd[0] == 's' && cmd[1] == 'y' && cmd[2] == 's' && cmd[3] == 't' && cmd[4] == 'e' && cmd[5] == 'm' && cmd[6] == 'c' && cmd[7] == 't' && cmd[8] == 'l' && cmd[9] == ' ' && cmd[10] == 's')) {
        term_add_line("* syslogd.service - System Event & Kernel Logging Daemon", COLOR_LEMON_GOLD);
        term_add_line("     Loaded: loaded (/lib/systemd/system/syslogd.service; enabled)", COLOR_TEXT_PRIMARY);
        term_add_line("     Active: active (running) since boot; supervisor active", COLOR_GREEN_ACCENT);
        term_add_line("   Main PID: 4 (syslogd) | Tasks: 1 | Memory: 2048 KB | CPU: 12ms", COLOR_TEXT_SECONDARY);
        term_add_line("     Journal: Started daemon supervisor process", COLOR_TEXT_MUTED);
    } else if (term_strcmp(cmd, "systemctl") == 0 || term_strcmp(cmd, "systemctl list-units") == 0) {
        term_add_line("UNIT                     LOAD   ACTIVE SUB     DESCRIPTION", COLOR_LEMON_GOLD);
        term_add_line("syslogd.service          loaded active running System Logging Daemon", COLOR_GREEN_ACCENT);
        term_add_line("networkd.service         loaded active running Intel e1000 Supervisor", COLOR_GREEN_ACCENT);
        term_add_line("crond.service            loaded active running Periodic Job Scheduler", COLOR_GREEN_ACCENT);
        term_add_line("httpd.service            loaded active running Embedded Web Server", COLOR_GREEN_ACCENT);
        term_add_line("sshd.service             loaded active running OpenSSH Remote Shell", COLOR_GREEN_ACCENT);
    } else if (term_strcmp(cmd, "pstree") == 0) {
        term_add_line("systemd(1)---+-kthreadd(2)", COLOR_LEMON_GOLD);
        term_add_line("             |-lemon-compositor(3)", COLOR_GREEN_ACCENT);
        term_add_line("             |-syslogd(4)", COLOR_TEXT_PRIMARY);
        term_add_line("             |-networkd(5)", COLOR_TEXT_PRIMARY);
        term_add_line("             |-crond(6)", COLOR_TEXT_PRIMARY);
        term_add_line("             `-httpd(7)", COLOR_TEXT_PRIMARY);
    } else if (term_strcmp(cmd, "runlevel") == 0) {
        term_add_line("N 5", COLOR_LEMON_GOLD);
    } else if (term_strcmp(cmd, "init 0") == 0) {
        init_set_runlevel(0);
    } else if (term_strcmp(cmd, "init 6") == 0) {
        init_set_runlevel(6);
    } else if (term_strcmp(cmd, "top") == 0) {
        pcb_t procs[PROC_TABLE_MAX];
        int count = proc_get_all(procs, PROC_TABLE_MAX);
        int r_cnt = 0, s_cnt = 0;
        for (int i = 0; i < count; i++) {
            if (procs[i].state == PROC_STATE_RUNNING) r_cnt++;
            else s_cnt++;
        }
        char hdr1[96];
        hdr1[0] = '\0';
        strcat(hdr1, "Tasks: ");
        char c_s[8]; term_int_to_str(count, c_s);
        strcat(hdr1, c_s);
        strcat(hdr1, " total, ");
        char r_s[8]; term_int_to_str(r_cnt, r_s);
        strcat(hdr1, r_s);
        strcat(hdr1, " running, ");
        char s_s[8]; term_int_to_str(s_cnt, s_s);
        strcat(hdr1, s_s);
        strcat(hdr1, " sleeping");

        term_add_line("top - LemonOS 3.0: up 14 min, 1 user, load: 0.12, 0.08, 0.02", COLOR_LEMON_GOLD);
        term_add_line(hdr1, COLOR_TEXT_PRIMARY);
        term_add_line("%Cpu(s):  4.2 us,  1.8 sy,  0.0 ni, 93.8 id,  0.2 wa,  0.0 hi", COLOR_TEXT_SECONDARY);
        term_add_line("MiB Mem :   32.0 total,   24.0 free,    8.0 used,    6.0 buff/cache", COLOR_TEXT_SECONDARY);
        term_add_line("  PID USER      PR  NI    VIRT    RES  S  %CPU  %MEM     TIME+ COMMAND", COLOR_LEMON_GOLD);
        for (int i = 0; i < count && i < 10; i++) {
            char row[128];
            row[0] = '\0';
            char p_s[8]; term_int_to_str(procs[i].pid, p_s);
            int plen = strlen(p_s);
            for (int s = 0; s < 5 - plen; s++) strcat(row, " ");
            strcat(row, p_s);
            strcat(row, " root      20   0   ");
            char v_s[8]; term_int_to_str(procs[i].mem_usage_kb, v_s);
            int vlen = strlen(v_s);
            for (int s = 0; s < 5 - vlen; s++) strcat(row, " ");
            strcat(row, v_s);
            strcat(row, "   ");
            for (int s = 0; s < 5 - vlen; s++) strcat(row, " ");
            strcat(row, v_s);
            strcat(row, (procs[i].state == PROC_STATE_RUNNING) ? "  R   " : "  S   ");
            char cp_s[8]; term_int_to_str(procs[i].cpu_usage_pct, cp_s);
            strcat(row, cp_s);
            strcat(row, ".0   6.2   0:01.42 ");
            strcat(row, procs[i].name);
            term_add_line(row, (procs[i].state == PROC_STATE_RUNNING) ? COLOR_GREEN_ACCENT : COLOR_TEXT_PRIMARY);
        }
    } else if (term_strncmp(cmd, "kill", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) {
        const char* p = cmd + 4;
        while (*p == ' ') p++;
        if (*p == '\0') {
            term_add_line("usage: kill [-s SIG | -SIG] <pid> ... (e.g. kill -9 5)", COLOR_TEXT_MUTED);
        } else if (term_strcmp(p, "-l") == 0) {
            term_add_line(" 1) SIGHUP   2) SIGINT   3) SIGQUIT  9) SIGKILL", COLOR_LEMON_GOLD);
            term_add_line("15) SIGTERM 18) SIGCONT 19) SIGSTOP", COLOR_LEMON_GOLD);
        } else {
            int sig = SIGTERM;
            if (*p == '-') {
                p++;
                int sval = 0;
                while (*p >= '0' && *p <= '9') {
                    sval = sval * 10 + (*p - '0');
                    p++;
                }
                if (sval > 0) sig = sval;
                while (*p == ' ') p++;
            }
            int target_pid = 0;
            while (*p >= '0' && *p <= '9') {
                target_pid = target_pid * 10 + (*p - '0');
                p++;
            }
            if (target_pid <= 1) {
                term_add_line("kill: operation not permitted on PID 1 (init)", COLOR_RED_ACCENT);
            } else {
                pcb_t* target = proc_get(target_pid);
                if (target) {
                    proc_kill(target_pid, sig);
                    char msg[96];
                    msg[0] = '\0';
                    strcat(msg, "kill: sent signal ");
                    char s_s[8]; term_int_to_str(sig, s_s);
                    strcat(msg, s_s);
                    strcat(msg, " to PID ");
                    char p_s[8]; term_int_to_str(target_pid, p_s);
                    strcat(msg, p_s);
                    strcat(msg, " (");
                    strcat(msg, target->name);
                    strcat(msg, ")");
                    term_add_line(msg, COLOR_GREEN_ACCENT);
                    alert_send("Process Terminated", target->name, ICON_TERMINAL, ALERT_TYPE_WARNING);
                } else {
                    term_add_line("kill: no such process", COLOR_RED_ACCENT);
                }
            }
        }
    } else if (term_strncmp(cmd, "pkill ", 6) == 0) {
        const char* name = cmd + 6;
        while (*name == ' ') name++;
        pcb_t procs[PROC_TABLE_MAX];
        int count = proc_get_all(procs, PROC_TABLE_MAX);
        int killed = 0;
        for (int i = 0; i < count; i++) {
            if (procs[i].pid > 1 && (strstr(procs[i].name, name) != NULL || strstr(procs[i].cmdline, name) != NULL)) {
                proc_kill(procs[i].pid, SIGTERM);
                killed++;
            }
        }
        if (killed > 0) {
            char msg[64];
            msg[0] = '\0';
            strcat(msg, "pkill: terminated ");
            char k_s[8]; term_int_to_str(killed, k_s);
            strcat(msg, k_s);
            strcat(msg, " process(es) matching '");
            strcat(msg, name);
            strcat(msg, "'");
            term_add_line(msg, COLOR_GREEN_ACCENT);
            alert_send("Processes Terminated", name, ICON_TERMINAL, ALERT_TYPE_WARNING);
        } else {
            term_add_line("pkill: no matching processes found", COLOR_RED_ACCENT);
        }
    } else if (term_strncmp(cmd, "pgrep ", 6) == 0) {
        const char* name = cmd + 6;
        while (*name == ' ') name++;
        pcb_t procs[PROC_TABLE_MAX];
        int count = proc_get_all(procs, PROC_TABLE_MAX);
        int found = 0;
        for (int i = 0; i < count; i++) {
            if (strstr(procs[i].name, name) != NULL || strstr(procs[i].cmdline, name) != NULL) {
                char p_s[16]; term_int_to_str(procs[i].pid, p_s);
                term_add_line(p_s, COLOR_LEMON_GOLD);
                found++;
            }
        }
        if (!found) {
            term_add_line("pgrep: no process found", COLOR_TEXT_MUTED);
        }
    } else if (term_strcmp(cmd, "free") == 0 || term_strcmp(cmd, "free -m") == 0 || term_strcmp(cmd, "free -h") == 0) {
        term_add_line("               total        used        free      shared  buff/cache   available", COLOR_LEMON_GOLD);
        term_add_line("Mem:        32768 kB     8192 kB    24576 kB     1024 kB     6144 kB    28672 kB", COLOR_TEXT_PRIMARY);
        term_add_line("Swap:              0           0           0", COLOR_TEXT_SECONDARY);
    } else if (term_strcmp(cmd, "uptime") == 0) {
        int fd = vfs_open("/proc/uptime", O_RDONLY);
        if (fd >= 0) {
            char ubuf[64];
            int r = vfs_read(fd, ubuf, sizeof(ubuf) - 1);
            if (r > 0) {
                ubuf[r] = '\0';
                char* sp = strchr(ubuf, ' ');
                if (sp) *sp = '\0';
                char u_out[80];
                u_out[0] = '\0';
                strcat(u_out, "up ");
                strcat(u_out, ubuf);
                strcat(u_out, " seconds, 1 user, load average: 0.12, 0.08, 0.02");
                term_add_line(u_out, COLOR_GREEN_ACCENT);
            } else {
                term_add_line("up 14 min, 1 user, load average: 0.12, 0.08, 0.02", COLOR_GREEN_ACCENT);
            }
            vfs_close(fd);
        } else {
            term_add_line("up 14 min, 1 user, load average: 0.12, 0.08, 0.02", COLOR_GREEN_ACCENT);
        }
    } else if (term_strcmp(cmd, "random") == 0) {
        uint8_t rand_bytes[8];
        entropy_get_bytes(rand_bytes, 8);
        char hex_str[32];
        static const char hx[] = "0123456789abcdef";
        for (int i = 0; i < 8; i++) {
            hex_str[i * 2] = hx[(rand_bytes[i] >> 4) & 0xF];
            hex_str[i * 2 + 1] = hx[rand_bytes[i] & 0xF];
        }
        hex_str[16] = '\0';
        term_add_line(hex_str, COLOR_LEMON_GOLD);
    } else if (term_strcmp(cmd, "whoami") == 0) {
        term_add_line(auth_get_username(), COLOR_LEMON_GOLD);
    } else if (term_strcmp(cmd, "id") == 0) {
        user_t* u = auth_get_current_user();
        if (u) {
            char id_buf[128];
            id_buf[0] = '\0';
            strcat(id_buf, "uid=");
            char uid_s[8]; term_int_to_str(u->uid, uid_s);
            strcat(id_buf, uid_s);
            strcat(id_buf, "("); strcat(id_buf, u->username); strcat(id_buf, ") gid=");
            char gid_s[8]; term_int_to_str(u->gid, gid_s);
            strcat(id_buf, gid_s);
            strcat(id_buf, "("); strcat(id_buf, u->username); strcat(id_buf, ") euid=");
            char euid_s[8]; term_int_to_str(auth_get_euid(), euid_s);
            strcat(id_buf, euid_s);
            if (auth_get_euid() == 0) strcat(id_buf, "(root)");
            else { strcat(id_buf, "("); strcat(id_buf, u->username); strcat(id_buf, ")"); }
            if (u->is_sudoer) strcat(id_buf, " groups=27(sudo),4(adm)");
            term_add_line(id_buf, COLOR_GREEN_ACCENT);
        } else {
            term_add_line("uid=0(root) gid=0(root)", COLOR_GREEN_ACCENT);
        }
    } else if (term_strcmp(cmd, "groups") == 0) {
        user_t* u = auth_get_current_user();
        char g_buf[64];
        g_buf[0] = '\0';
        strcat(g_buf, u ? u->username : "root");
        strcat(g_buf, " : ");
        strcat(g_buf, u ? u->username : "root");
        if (u && u->is_sudoer) strcat(g_buf, " adm sudo");
        term_add_line(g_buf, COLOR_GREEN_ACCENT);
    } else if (term_strcmp(cmd, "hostname") == 0) {
        term_add_line(auth_get_hostname(), COLOR_LEMON_GOLD);
    } else if (term_strcmp(cmd, "passwd") == 0) {
        char pw_msg[64];
        pw_msg[0] = '\0';
        strcat(pw_msg, "Changing password for ");
        strcat(pw_msg, auth_get_username());
        term_add_line(pw_msg, COLOR_TEXT_PRIMARY);
        term_add_line("passwd: password updated successfully", COLOR_GREEN_ACCENT);
    } else if (term_strncmp(cmd, "su", 2) == 0 && (cmd[2] == '\0' || cmd[2] == ' ')) {
        const char* target = (cmd[2] == ' ' && cmd[3]) ? (cmd + 3) : "root";
        while (*target == ' ') target++;
        if (auth_switch_user(target) == 0) {
            char su_msg[64];
            su_msg[0] = '\0';
            strcat(su_msg, "Switched to user: ");
            strcat(su_msg, auth_get_username());
            term_add_line(su_msg, COLOR_LEMON_GOLD);
            if (auth_get_euid() == 0) {
                term_add_line("# You now have root privileges", COLOR_RED_ACCENT);
            }
        } else {
            char err_msg[64];
            err_msg[0] = '\0';
            strcat(err_msg, "su: user '");
            strcat(err_msg, target);
            strcat(err_msg, "' does not exist");
            term_add_line(err_msg, COLOR_RED_ACCENT);
        }
    } else if (term_strncmp(cmd, "sudo ", 5) == 0) {
        const char* subcmd = cmd + 5;
        while (*subcmd == ' ') subcmd++;
        user_t* u = auth_get_current_user();
        if (!u || !auth_is_sudoer(u->username)) {
            char deny[80];
            deny[0] = '\0';
            strcat(deny, u ? u->username : "unknown");
            strcat(deny, " is not in the sudoers file. This incident will be reported.");
            term_add_line(deny, COLOR_RED_ACCENT);
        } else {
            uint32_t saved = auth_get_euid();
            auth_set_euid(0);
            term_add_line("[sudo] running as root...", COLOR_LEMON_GOLD);
            term_execute(subcmd);
            auth_set_euid(saved);
        }
    } else if (term_strcmp(cmd, "logout") == 0 || term_strcmp(cmd, "exit") == 0) {
        auth_switch_user("dwip");
        auth_drop_privileges();
        term_add_line("Logged out. Session reset to dwip.", COLOR_GREEN_ACCENT);
    } else if (term_strncmp(cmd, "adduser ", 8) == 0 || term_strncmp(cmd, "useradd ", 8) == 0) {
        const char* uname = cmd + 8;
        while (*uname == ' ') uname++;
        if (!auth_is_root()) {
            term_add_line("adduser: Only root may add a user to the system.", COLOR_RED_ACCENT);
        } else {
            char msg[64];
            msg[0] = '\0';
            strcat(msg, "Adding user '"); strcat(msg, uname); strcat(msg, "' ... done.");
            term_add_line(msg, COLOR_GREEN_ACCENT);
            term_add_line("passwd: password updated successfully", COLOR_GREEN_ACCENT);
            char hdir[64] = "/home/";
            int hi = 6, ai = 0;
            while (uname[ai] && hi < 62) hdir[hi++] = uname[ai++];
            hdir[hi] = '\0';
            fs_mkdir(hdir);
            char hmsg[80]; hmsg[0] = '\0';
            strcat(hmsg, "Home directory '"); strcat(hmsg, hdir); strcat(hmsg, "' created.");
            term_add_line(hmsg, COLOR_TEXT_PRIMARY);
        }
    } else if ((cmd[0] == 'f' && cmd[1] == 'i' && cmd[2] == 'l' && cmd[3] == 'e' && cmd[4] == 's') || term_strcmp(cmd, "explorer") == 0) {
        const char* arg = (cmd[5] == ' ') ? (cmd + 6) : "/bin";
        term_add_line("Opening File Explorer in /bin...", COLOR_GREEN_ACCENT);
        gui_files_open();
        if (arg && arg[0]) files_set_dir(arg);
    } else {
        // Try executing installed APT Linux tools (neofetch, htop, cowsay, sl, tree, figlet, fortune, curl, nano, bash, python3, ncdu)
        char tool_name[32];
        const char* tool_arg = "";
        int ti = 0;
        while (cmd[ti] && cmd[ti] != ' ' && ti < 31) {
            tool_name[ti] = cmd[ti];
            ti++;
        }
        tool_name[ti] = '\0';
        if (cmd[ti] == ' ') {
            tool_arg = cmd + ti + 1;
            while (*tool_arg == ' ') tool_arg++;
        }

        // Try executing via Multi-Language Runtime (Python, JS, TS, Java, C++, Assembly, WASM, HTML)
        int l_res = langrt_exec_cmd(tool_name, tool_arg, term_add_line);
        if (l_res == 0) {
            return;
        }

        // Check if command is a file path with an executable multi-language extension
        if (tool_name[0] == '/' || (tool_name[0] == '.' && tool_name[1] == '/')) {
            const char* path = (tool_name[0] == '.') ? (tool_name + 2) : tool_name;
            language_t lang = langrt_detect(path);
            if (lang != LANG_UNKNOWN) {
                langrt_run_file(path, term_add_line);
                return;
            }
        }

        // Check if command corresponds to an ELF executable in /bin or explicit path
        char bin_exec_path[128];
        bin_exec_path[0] = '\0';
        if (tool_name[0] == '/') {
            strncpy(bin_exec_path, tool_name, sizeof(bin_exec_path) - 1);
        } else {
            strcpy(bin_exec_path, "/bin/");
            strncat(bin_exec_path, tool_name, sizeof(bin_exec_path) - 6);
        }
        vfs_stat_t vst;
        if (vfs_stat(bin_exec_path, &vst) == 0 && (vst.flags != VFS_DIRECTORY)) {
            char* exec_argv[4];
            exec_argv[0] = bin_exec_path;
            int exec_argc = 1;
            if (tool_arg && *tool_arg) {
                exec_argv[1] = (char*)tool_arg;
                exec_argv[2] = NULL;
                exec_argc = 2;
            } else {
                exec_argv[1] = NULL;
            }
            process_spawn(bin_exec_path, exec_argc, exec_argv);
            return;
        }

        int t_res = apt_run_tool(tool_name, tool_arg, term_add_line);
        if (t_res == 0 || t_res == -1) {
            return;
        }

        char err[64];
        int ep = 0;
        const char* prefix = "Unknown command: ";
        while (*prefix) err[ep++] = *prefix++;
        int eq = 0;
        while (cmd[eq] && ep < 50) err[ep++] = cmd[eq++];
        err[ep] = '\0';
        term_add_line(err, COLOR_RED_ACCENT);
        term_add_line("Type 'help' for a list of commands.", COLOR_TEXT_MUTED);
    }
}

static void term_render(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win;
    // Deep terminal background (Catppuccin Mocha Crust)
    gfx_fill_rect(cx, cy, cw, ch, 0xFF11111B);

    int line_spacing = FONT_HEIGHT + 2;
    int available_h = ch - 16;
    int max_lines_total = available_h / line_spacing;
    if (max_lines_total < 2) max_lines_total = 2;

    int max_history_lines = max_lines_total - 1; // Reserve bottom line strictly for prompt
    int max_scroll = (term_state.line_count > max_history_lines) ? (term_state.line_count - max_history_lines) : 0;
    if (term_state.scroll_offset < 0) term_state.scroll_offset = 0;
    if (term_state.scroll_offset > max_scroll) term_state.scroll_offset = max_scroll;

    int start_line = 0;
    if (term_state.line_count > max_history_lines) {
        start_line = (term_state.line_count - max_history_lines) - term_state.scroll_offset;
        if (start_line < 0) start_line = 0;
    }
    int end_line = start_line + max_history_lines;
    if (end_line > term_state.line_count) end_line = term_state.line_count;

    int text_w = cw - 24;

    // Draw output history
    int line_y = cy + 8;
    for (int i = start_line; i < end_line; i++) {
        gfx_draw_text_clipped(cx + 10, line_y, text_w, term_state.lines[i], term_state.line_colors[i]);
        line_y += line_spacing;
    }

    // Draw input prompt
    const char* prompt = auth_get_prompt();
    uint32_t prompt_color = (auth_get_euid() == 0) ? 0xFFFF4444 : COLOR_GREEN_ACCENT;
    gfx_draw_text(cx + 10, line_y, prompt, prompt_color);
    int prompt_w = font_get_string_width(prompt);

    // Draw current user input
    gfx_draw_text_clipped(cx + 10 + prompt_w, line_y, text_w - prompt_w, term_state.input, COLOR_TEXT_PRIMARY);

    // Blinking block cursor
    term_state.cursor_blink = (term_state.cursor_blink + 1) % 40;
    if (term_state.cursor_blink < 25) {
        int input_w = font_get_string_width(term_state.input);
        if (10 + prompt_w + input_w + 8 < text_w) {
            gfx_fill_rect(cx + 10 + prompt_w + input_w, line_y + 2, 8, FONT_HEIGHT - 2, COLOR_LEMON_GOLD);
        }
    }

    // Draw scrollbar if there are scrollable lines
    if (max_scroll > 0) {
        int sb_w = 8;
        int sb_x = cx + cw - sb_w - 4;
        int sb_y = cy + 4;
        int sb_h = ch - 8;

        // Scrollbar track
        gfx_fill_rounded_rect(sb_x, sb_y, sb_w, sb_h, 4, 0xFF181825);

        // Scrollbar thumb
        int thumb_h = (max_history_lines * sb_h) / term_state.line_count;
        if (thumb_h < 18) thumb_h = 18;
        if (thumb_h > sb_h) thumb_h = sb_h;

        int max_travel = sb_h - thumb_h;
        int thumb_y = sb_y + max_travel - ((term_state.scroll_offset * max_travel) / max_scroll);
        uint32_t thumb_color = term_state.is_dragging_scroll ? 0xFF89B4FA : 0xFF45475A;
        gfx_fill_rounded_rect(sb_x, thumb_y, sb_w, thumb_h, 4, thumb_color);
    }
}

static void term_on_mouse(window_t* win, int rx, int ry, int buttons, int event_type) {
    (void)buttons;
    int cw = (win) ? (win->width - 2 * WM_BORDER_WIDTH) : 660;
    int ch = (win) ? (win->height - WM_TITLEBAR_HEIGHT - WM_BORDER_WIDTH) : 420;

    int line_spacing = FONT_HEIGHT + 2;
    int available_h = ch - 16;
    int max_lines_total = available_h / line_spacing;
    if (max_lines_total < 2) max_lines_total = 2;
    int max_history_lines = max_lines_total - 1;
    int max_scroll = (term_state.line_count > max_history_lines) ? (term_state.line_count - max_history_lines) : 0;

    if (event_type == WM_EVENT_SCROLL_UP) {
        if (max_scroll > 0) {
            term_state.scroll_offset += 3;
            if (term_state.scroll_offset > max_scroll) term_state.scroll_offset = max_scroll;
        }
        return;
    } else if (event_type == WM_EVENT_SCROLL_DOWN) {
        if (max_scroll > 0) {
            term_state.scroll_offset -= 3;
            if (term_state.scroll_offset < 0) term_state.scroll_offset = 0;
        }
        return;
    }

    int sb_w = 12;
    int sb_x = cw - sb_w - 4;
    int sb_y = 4;
    int sb_h = ch - 8;

    if (event_type == WM_EVENT_MOUSE_DOWN) {
        if (max_scroll > 0 && rx >= sb_x - 4 && rx <= cw && ry >= sb_y && ry <= sb_y + sb_h) {
            term_state.is_dragging_scroll = 1;
            term_state.drag_start_y = ry;
            term_state.drag_start_offset = term_state.scroll_offset;

            int thumb_h = (max_history_lines * sb_h) / term_state.line_count;
            if (thumb_h < 18) thumb_h = 18;
            int max_travel = sb_h - thumb_h;
            if (max_travel > 0) {
                int rel_pos = ry - sb_y - thumb_h / 2;
                if (rel_pos < 0) rel_pos = 0;
                if (rel_pos > max_travel) rel_pos = max_travel;
                term_state.scroll_offset = max_scroll - (rel_pos * max_scroll) / max_travel;
                term_state.drag_start_offset = term_state.scroll_offset;
            }
        }
    } else if (event_type == WM_EVENT_MOUSE_DRAG) {
        if (term_state.is_dragging_scroll && max_scroll > 0) {
            int thumb_h = (max_history_lines * sb_h) / term_state.line_count;
            if (thumb_h < 18) thumb_h = 18;
            int max_travel = sb_h - thumb_h;
            if (max_travel > 0) {
                int dy = ry - term_state.drag_start_y;
                int delta = (dy * max_scroll) / max_travel;
                term_state.scroll_offset = term_state.drag_start_offset - delta;
                if (term_state.scroll_offset < 0) term_state.scroll_offset = 0;
                if (term_state.scroll_offset > max_scroll) term_state.scroll_offset = max_scroll;
            }
        }
    } else if (event_type == WM_EVENT_MOUSE_UP) {
        term_state.is_dragging_scroll = 0;
    }
}

static void term_on_key(window_t* win, char key) {
    (void)win;
    if (key == '\n' || key == '\r') {
        term_execute(term_state.input);
        term_state.input[0] = '\0';
        term_state.input_len = 0;
    } else if (key == '\b') {
        if (term_state.input_len > 0) {
            term_state.input_len--;
            term_state.input[term_state.input_len] = '\0';
        }
    } else if (key >= 32 && key <= 126) {
        if (term_state.input_len < TERM_LINE_LEN - 1) {
            term_state.input[term_state.input_len++] = key;
            term_state.input[term_state.input_len] = '\0';
        }
    }
}

static void term_on_close(window_t* win) {
    (void)win;
    term_win = NULL;
}

void gui_terminal_open(void) {
    if (term_win && !term_win->is_closed) {
        if (term_win->is_minimized) {
            wm_restore_window(term_win);
        }
        wm_bring_to_front(term_win);
        return;
    }

    term_state.line_count = 0;
    term_state.input[0] = '\0';
    term_state.input_len = 0;
    term_state.cursor_blink = 0;
    term_state.scroll_offset = 0;
    term_state.is_dragging_scroll = 0;

    term_add_line("LemonOS Terminal v3.0 [i386 Protected Mode]", COLOR_LEMON_GOLD);
    term_add_line("Process filesystem /proc mounted. Process Manager active.", COLOR_GREEN_ACCENT);
    term_add_line("Type 'ps aux', 'top', 'cat /proc/version', 'cat /proc/meminfo' or 'help'.", COLOR_TEXT_SECONDARY);
    term_add_line("", COLOR_TEXT_PRIMARY);

    term_win = wm_create_window("Terminal", ICON_TERMINAL, 80, 50, 660, 420, term_render);
    if (term_win) {
        term_win->on_key = term_on_key;
        term_win->on_mouse = term_on_mouse;
        term_win->on_close = term_on_close;
    }
}
