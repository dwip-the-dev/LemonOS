// BusyBox v1.36.1 Multi-Call Swiss Army Knife for LemonOS
// Author: Dwip (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "busybox.h"
#include "filesystem.h"
#include "vfs.h"
#include "posix_time.h"
#include "rtc.h"
#include "pmm.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

static void busybox_usage(void) {
    printf("BusyBox v1.36.1 (LemonOS Baremetal Multi-call Binary)\n\n");
    printf("Usage: busybox [function [arguments]...]\n");
    printf("   or: [function] [arguments]...\n\n");
    printf("Currently defined functions:\n");
    printf("   awk, cat, cp, date, df, echo, env, false, free, grep,\n");
    printf("   head, id, ls, mkdir, mv, rm, sed, sh, sleep, tail,\n");
    printf("   touch, true, uname, uptime, wc, which, whoami, yes\n");
}

int busybox_main(int argc, char** argv) {
    if (argc < 2) {
        busybox_usage();
        return 0;
    }

    const char* applet = argv[1];
    int sub_argc = argc - 1;
    char** sub_argv = &argv[1];

    if (strcmp(applet, "--list") == 0 || strcmp(applet, "--help") == 0) {
        busybox_usage();
        return 0;
    }

    // uname
    if (strcmp(applet, "uname") == 0) {
        int all = 0;
        for (int i = 1; i < sub_argc; i++) {
            if (strcmp(sub_argv[i], "-a") == 0) all = 1;
        }
        if (all) {
            printf("Linux lemonos 6.6.0-lemon #1 SMP PREEMPT_DYNAMIC x86 GNU/Linux\n");
        } else {
            printf("LemonOS\n");
        }
        return 0;
    }

    // whoami
    if (strcmp(applet, "whoami") == 0) {
        printf("dwip\n");
        return 0;
    }

    // id
    if (strcmp(applet, "id") == 0) {
        printf("uid=1000(dwip) gid=1000(dwip) groups=1000(dwip),27(sudo),100(users)\n");
        return 0;
    }

    // echo
    if (strcmp(applet, "echo") == 0) {
        int newline = 1;
        int start = 1;
        if (sub_argc > 1 && strcmp(sub_argv[1], "-n") == 0) {
            newline = 0;
            start = 2;
        }
        for (int i = start; i < sub_argc; i++) {
            printf("%s%s", sub_argv[i], (i < sub_argc - 1) ? " " : "");
        }
        if (newline) printf("\n");
        return 0;
    }

    // env
    if (strcmp(applet, "env") == 0) {
        printf("USER=dwip\n");
        printf("HOME=/home/dwip\n");
        printf("SHELL=/bin/sh\n");
        printf("TERM=lemon-vga\n");
        printf("PATH=/bin:/usr/bin:/sbin:/usr/sbin\n");
        printf("OS=LemonOS\n");
        return 0;
    }

    // which
    if (strcmp(applet, "which") == 0) {
        if (sub_argc < 2) return 1;
        printf("/bin/%s\n", sub_argv[1]);
        return 0;
    }

    // cat
    if (strcmp(applet, "cat") == 0) {
        if (sub_argc < 2) return 1;
        for (int i = 1; i < sub_argc; i++) {
            char* c = fs_read_file(sub_argv[i]);
            if (c) printf("%s\n", c);
            else printf("cat: %s: No such file or directory\n", sub_argv[i]);
        }
        return 0;
    }

    // head
    if (strcmp(applet, "head") == 0) {
        if (sub_argc < 2) return 1;
        int nlines = 10;
        int fidx = 1;
        if (strcmp(sub_argv[1], "-n") == 0 && sub_argc > 3) {
            nlines = atoi(sub_argv[2]);
            fidx = 3;
        }
        char* c = fs_read_file(sub_argv[fidx]);
        if (!c) {
            printf("head: cannot open '%s' for reading\n", sub_argv[fidx]);
            return 1;
        }
        int count = 0;
        while (*c && count < nlines) {
            putchar(*c);
            if (*c == '\n') count++;
            c++;
        }
        return 0;
    }

    // tail
    if (strcmp(applet, "tail") == 0) {
        if (sub_argc < 2) return 1;
        int nlines = 10;
        int fidx = 1;
        if (strcmp(sub_argv[1], "-n") == 0 && sub_argc > 3) {
            nlines = atoi(sub_argv[2]);
            fidx = 3;
        }
        char* c = fs_read_file(sub_argv[fidx]);
        if (!c) {
            printf("tail: cannot open '%s' for reading\n", sub_argv[fidx]);
            return 1;
        }
        // Count total lines
        int total = 0;
        for (const char* p = c; *p; p++) if (*p == '\n') total++;
        int skip = total > nlines ? total - nlines : 0;
        int passed = 0;
        while (*c) {
            if (passed >= skip) putchar(*c);
            if (*c == '\n') passed++;
            c++;
        }
        return 0;
    }

    // wc (lines, words, bytes)
    if (strcmp(applet, "wc") == 0) {
        if (sub_argc < 2) return 1;
        char* c = fs_read_file(sub_argv[1]);
        if (!c) {
            printf("wc: %s: No such file or directory\n", sub_argv[1]);
            return 1;
        }
        int lines = 0, words = 0, bytes = 0;
        int in_word = 0;
        for (const char* p = c; *p; p++) {
            bytes++;
            if (*p == '\n') lines++;
            if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                words++;
            }
        }
        printf(" %d  %d %d %s\n", lines, words, bytes, sub_argv[1]);
        return 0;
    }

    // grep
    if (strcmp(applet, "grep") == 0) {
        if (sub_argc < 3) {
            printf("Usage: busybox grep <pattern> <file>\n");
            return 1;
        }
        const char* pattern = sub_argv[1];
        char* c = fs_read_file(sub_argv[2]);
        if (!c) {
            printf("grep: %s: No such file or directory\n", sub_argv[2]);
            return 1;
        }
        char line[256];
        const char* p = c;
        while (*p) {
            int idx = 0;
            while (*p && *p != '\n' && *p != '\r' && idx < 255) line[idx++] = *p++;
            line[idx] = '\0';
            if (*p == '\r') p++;
            if (*p == '\n') p++;
            if (strstr(line, pattern)) {
                printf("%s\n", line);
            }
        }
        return 0;
    }

    // sed (stream editor find and replace: sed 's/foo/bar/g' file)
    if (strcmp(applet, "sed") == 0) {
        if (sub_argc < 3) {
            printf("Usage: busybox sed 's/find/replace/g' <file>\n");
            return 1;
        }
        const char* script = sub_argv[1];
        char* c = fs_read_file(sub_argv[2]);
        if (!c) {
            printf("sed: %s: No such file or directory\n", sub_argv[2]);
            return 1;
        }
        if (strncmp(script, "s/", 2) == 0) {
            char find[64] = ""; char repl[64] = "";
            const char* slash1 = script + 2;
            const char* slash2 = strchr(slash1, '/');
            if (slash2) {
                int flen = slash2 - slash1;
                strncpy(find, slash1, flen); find[flen] = '\0';
                const char* slash3 = strchr(slash2 + 1, '/');
                if (slash3) {
                    int rlen = slash3 - (slash2 + 1);
                    strncpy(repl, slash2 + 1, rlen); repl[rlen] = '\0';
                }
            }
            if (find[0]) {
                const char* p = c;
                while (*p) {
                    const char* match = strstr(p, find);
                    if (match) {
                        while (p < match) putchar(*p++);
                        printf("%s", repl);
                        p += strlen(find);
                    } else {
                        putchar(*p++);
                    }
                }
                return 0;
            }
        }
        printf("%s\n", c);
        return 0;
    }

    // df
    if (strcmp(applet, "df") == 0) {
        printf("Filesystem     1K-blocks      Used Available Use%% Mounted on\n");
        printf("/dev/ramdisk0      65536     14200     51336  22%% /\n");
        printf("devfs               1024         4      1020   1%% /dev\n");
        printf("procfs              1024        12      1012   2%% /proc\n");
        return 0;
    }

    // free
    if (strcmp(applet, "free") == 0) {
        uint32_t free_pages = pmm_get_free_frames();
        uint32_t total_pages = pmm_get_total_frames();
        uint32_t used_pages = total_pages - free_pages;
        printf("               total        used        free      shared  buff/cache   available\n");
        printf("Mem:        %8u    %8u    %8u           0        2048    %8u\n",
               total_pages * 4, used_pages * 4, free_pages * 4, free_pages * 4);
        printf("Swap:              0           0           0\n");
        return 0;
    }

    // uptime
    if (strcmp(applet, "uptime") == 0) {
        uint64_t ms = timer_get_ticks_ms();
        uint32_t sec = ms / 1000;
        uint32_t min = sec / 60;
        uint32_t hrs = min / 60;
        printf(" up %u:%02u,  1 user,  load average: 0.04, 0.03, 0.00\n", hrs, min % 60);
        return 0;
    }

    // date
    if (strcmp(applet, "date") == 0) {
        int d = 1, m = 1, y = 2026;
        int h = 0, min = 0, s = 0;
        get_rtc_date(&d, &m, &y);
        get_rtc_time(&h, &min, &s);
        printf("%04d-%02d-%02d %02d:%02d:%02d UTC\n", y, m, d, h, min, s);
        return 0;
    }

    // true / false / sleep
    if (strcmp(applet, "true") == 0) return 0;
    if (strcmp(applet, "false") == 0) return 1;
    if (strcmp(applet, "sleep") == 0) {
        int sec = (sub_argc > 1) ? atoi(sub_argv[1]) : 1;
        timer_sleep_ms(sec * 1000);
        return 0;
    }

    printf("busybox: %s: applet not found\n", applet);
    return 1;
}
