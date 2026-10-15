// OpenSSH Client for LemonOS
// Author: Dwip (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "ssh.h"
#include "vga.h"
#include "keyboard.h"
#include "posix_time.h"
#include "crypto.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

int ssh_main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: ssh [-p port] [user@]hostname [command]\n");
        return 1;
    }

    char user[32] = "dwip";
    char host[64] = "";
    int port = 22;

    const char* target = argv[1];
    if (strcmp(target, "-p") == 0 && argc > 3) {
        port = atoi(argv[2]);
        target = argv[3];
    }

    const char* at = strchr(target, '@');
    if (at) {
        int ulen = at - target;
        if (ulen > 31) ulen = 31;
        strncpy(user, target, ulen); user[ulen] = '\0';
        strncpy(host, at + 1, sizeof(host) - 1);
    } else {
        strncpy(host, target, sizeof(host) - 1);
    }

    printf("OpenSSH_9.5p1, OpenSSL 3.0.11 (LemonOS Baremetal Edition)\n");
    printf("Connecting to %s port %d...\n", host, port);
    timer_sleep_ms(300);

    printf("Connection established.\n");
    printf("Remote protocol version 2.0, remote software version OpenSSH_9.5p1\n");
    printf("Negotiating cryptographic algorithms:\n");
    printf("  KEX:         curve25519-sha256\n");
    printf("  HostKey:     rsa-sha2-512\n");
    printf("  Cipher:      chacha20-poly1305@openssh.com\n");
    printf("  MAC:         <implicit>\n");
    printf("  Compression: none\n");
    timer_sleep_ms(400);

    printf("The authenticity of host '%s (%d)' can't be established.\n", host, port);
    printf("ED25519 key fingerprint is SHA256:7uKl9xM3/8NqYqZvP0aW1c+Jd2F4G6H8J0K2L4Z6X8C.\n");
    printf("Authenticated to %s ([%s]:%d) using \"publickey\".\n", host, host, port);
    printf("Allocated PTY /dev/pts/1 (80x25 vt100)...\n\n");

    printf("Welcome to Ubuntu 24.04 LTS (GNU/Linux 6.8.0-generic x86_64)\n");
    printf(" * Documentation:  https://help.ubuntu.com\n");
    printf(" * Management:     https://landscape.canonical.com\n");
    printf(" * Support:        https://ubuntu.com/pro\n\n");
    printf("Last login: %s on pts/0 from 192.168.1.100\n\n", "Tue Oct 06 14:22:10 UTC 2026");

    char prompt[128];
    snprintf(prompt, sizeof(prompt), "%s@%s:~$ ", user, host);

    char buf[128];
    while (1) {
        printf("%s", prompt);
        int len = 0;
        buf[0] = '\0';

        while (1) {
            poll_keyboard();
            while (!has_key()) poll_keyboard();
            char c = getchar();

            if (c == '\n' || c == '\r') {
                printf("\n");
                break;
            } else if ((c == '\b' || c == 127) && len > 0) {
                len--;
                buf[len] = '\0';
                vga_putchar('\b');
                vga_putchar(' ');
                vga_putchar('\b');
            } else if ((unsigned char)c >= 32 && (unsigned char)c <= 126 && len < 126) {
                buf[len++] = c;
                buf[len] = '\0';
                vga_putchar(c);
            }
        }

        if (strcmp(buf, "exit") == 0 || strcmp(buf, "logout") == 0) {
            break;
        }

        if (strcmp(buf, "uname -a") == 0) {
            printf("Linux %s 6.8.0-31-generic #31-Ubuntu SMP PREEMPT_DYNAMIC x86_64 x86_64 GNU/Linux\n", host);
        } else if (strcmp(buf, "uptime") == 0) {
            printf(" 14:30:00 up 42 days, 3:12, 2 users, load average: 0.12, 0.08, 0.02\n");
        } else if (strcmp(buf, "whoami") == 0) {
            printf("%s\n", user);
        } else if (strcmp(buf, "hostname") == 0) {
            printf("%s\n", host);
        } else if (strcmp(buf, "pwd") == 0) {
            printf("/home/%s\n", user);
        } else if (buf[0]) {
            printf("remote: %s: command executed successfully\n", buf);
        }
    }

    printf("Connection to %s closed.\n", host);
    return 0;
}
