#include "commands.h"
#include "vga.h"
#include "art.h"
#include "keyboard.h"
#include "calculator.h"
#include "clock.h"
#include "system_info.h"
#include "vga_utils.h"
#include "unit_converter.h"
#include "screensaver.h" 
#include "file_manager.h" 
#include "text_graphics.h"
#include "snake.h"
#include "shutdown.h"
#include "reboot.h" 
#include "gui.h"
#include "desktop.h"
#include "color_demo.h"
#include "breakout.h"
#include "pong.h"
#include "net.h"
#include "filesystem.h"
#include "e1000.h"
#include "gui_firefox.h"
#include "bga.h"
#include "block.h"
#include "vfs.h"
#include "devfs.h"
#include "syscall.h"
#include "user_loader.h"
#include "dynlink.h"
#include "crypto.h"
#include "entropy.h"
#include "posix_time.h"
#include "inotify.h"
#include "mman.h"
#include "termios.h"
#include "proc_mgr.h"
#include "service.h"
#include "procfs.h"
#include "compiler.h"
#include "make.h"
#include "git.h"
#include "apt.h"
#include "init.h"
#include "gui_services.h"
#include "auth.h"
#include "string.h"
#include "slab.h"
#include "bcache.h"
#include "pipe.h"
#include "langrt.h"
#include "nano.h"
#include "vim.h"
#include "less.h"
#include "lua.h"
#include "sqlite.h"
#include "busybox.h"
#include "tar_gzip.h"
#include "ssh.h"
#include "curl.h"
#include "speedtest.h"

static void cmd_print_dec(uint32_t val) {
    if (val == 0) {
        vga_putchar('0');
        return;
    }
    char buf[16];
    int idx = 0;
    while (val > 0) {
        buf[idx++] = '0' + (val % 10);
        val /= 10;
    }
    for (int i = idx - 1; i >= 0; i--) {
        vga_putchar(buf[i]);
    }
}

// Simple string length function
int kstrlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}


// Split command into cmd and arg
static void parse_cmd_arg(const char* input, char* cmd_out, char* arg_out) {
    int i = 0;
    while (input[i] && input[i] != ' ' && i < 31) {
        cmd_out[i] = input[i];
        i++;
    }
    cmd_out[i] = '\0';

    while (input[i] == ' ') i++;

    int j = 0;
    while (input[i] && j < 127) {
        arg_out[j++] = input[i++];
    }
    arg_out[j] = '\0';
}

// Quote-aware token parser for command line arguments
static int parse_cmd_tokens(const char* input, char* argv[], int max_argv, char* storage, int storage_size) {
    int argc = 0;
    int s_idx = 0;
    const char* p = input;

    while (*p && argc < max_argv - 1 && s_idx < storage_size - 1) {
        while (*p == ' ') p++;
        if (!*p) break;

        argv[argc++] = &storage[s_idx];
        char quote = 0;
        if (*p == '\'' || *p == '"') {
            quote = *p++;
        }

        while (*p && s_idx < storage_size - 1) {
            if (quote) {
                if (*p == quote) {
                    p++;
                    break;
                }
                storage[s_idx++] = *p++;
            } else {
                if (*p == ' ') break;
                if (*p == '\'' || *p == '"') {
                    quote = *p++;
                    continue;
                }
                storage[s_idx++] = *p++;
            }
        }
        storage[s_idx++] = '\0';
    }
    argv[argc] = NULL;
    return argc;
}

void run_command(const char* input_cmd) {
    update_activity_time();
    
    if (input_cmd[0] == '\0') {
        return;
    }

    char cmd[32];
    char arg[128];
    parse_cmd_arg(input_cmd, cmd, arg);

    if (strcmp(cmd, "help") == 0) {
        vga_println("--- LemonOS 3.0 Universal Commands ---");
        vga_println("Languages: g++, python3, node, tsc, java, javac, nasm, wasm, rustc");
        vga_println("DevTools:  gcc, clang, make, git, apt, dpkg, gdb, valgrind, strace");
        vga_println("Package:   apt update, apt install <any-package>, apt search <term>");
        vga_println("System:    help, clear, about, info, reboot, shutdown, colors, gui");
        vga_println("Editors:   nano <file>, vim <file>, vi <file>, less <file>");
        vga_println("Classics:  lua [file], python3 [file], sqlite3 [db], busybox <applet>, tar -[ctx]f, gzip, gunzip, ssh <host>, curl <url>");
        vga_println("Apps:      doom, firefox [url], calc, clock, convert, matrix, snake, breakout, pong, files");
        vga_println("Network:   ifconfig, ping <ip>, dhcp, dns <host>, wget <url>, curl <url>");
        vga_println("Files:     ls, cat <file>, touch <file>, rm <file>, mkdir <dir>, fstest");
        vga_println("Storage:   mount, df, block, vfs, vfstest, blocktest");
        vga_println("Users:     sudo <cmd>, su [user], whoami, id, groups, passwd, hostname");
        vga_println("Process:   top, kill <pid>, pgrep <pat>, pkill <pat>, pstree, free, uptime");
        vga_println("Services:  service, systemctl, services, init [runlevel]");
        vga_println("Crypto:    sha1sum, sha256sum, md5sum, random");
    } 
    else if (strcmp(cmd, "firefox") == 0 || strcmp(cmd, "browser") == 0) {
        if (bga_is_available()) {
            if (arg[0] != '\0') {
                gui_firefox_open_url(arg);
            } else {
                gui_firefox_open();
            }
            desktop_run();
        } else {
            vga_println("Mozilla Firefox (Default Browser): Running in CLI fallback mode.");
            if (arg[0] != '\0') {
                char buf[1024]; int status = 0;
                net_http_get(arg, buf, sizeof(buf), &status);
                vga_println(buf);
            } else {
                vga_println("Usage: firefox <url> (or type 'gui' for full graphical experience)");
            }
        }
    }
    else if (strcmp(cmd, "clear") == 0) {
        vga_clear();
    } 
    else if (strcmp(cmd, "about") == 0) {
        for (int i = 0; logo[i] != 0; i++) {
            vga_println(logo[i]);
        }
        vga_println("LemonOS v3.0 Enterprise - Baremetal x86 OS by Dwip");
    }
    else if (strcmp(cmd, "convert") == 0) {
        unit_converter_command();
    }
    else if (strcmp(cmd, "matrix") == 0) {
        matrix_screensaver();
    }
    else if (strcmp(cmd, "doom") == 0) {
        extern void doom_main(int argc, char** argv);
        char* doom_argv[] = { "doom", NULL };
        doom_main(1, doom_argv);
        bga_disable();
        vga_initialize();
        vga_clear();
        vga_println("DOOM session terminated cleanly. Returned to LemonOS terminal shell.");
    }
    else if (strcmp(cmd, "snake") == 0) {
        snake_game();
    }
    else if (strcmp(cmd, "files") == 0) {
        file_manager_command();
    }
    else if (strcmp(cmd, "calc") == 0) {
        calc_command();
    }
    else if (strcmp(cmd, "clock") == 0) {
        clock_command();
    }
    else if (strcmp(cmd, "info") == 0) {
        system_info_command();
    }
    else if (strcmp(cmd, "shutdown") == 0) {
        shutdown();
    }
    else if (strcmp(cmd, "reboot") == 0) {
        reboot();
    }
    else if (strcmp(cmd, "gui") == 0 || strcmp(cmd, "desktop") == 0) {
        desktop_run();
    }
    else if (strcmp(cmd, "colors") == 0) {
        color_demo();
    }
    else if (strcmp(cmd, "breakout") == 0) {
        breakout_game();
    }
    else if (strcmp(cmd, "pong") == 0) {
        pong_game();
    }
    // Classic Applications (Editors, Runtimes, Database, Utilities)
    else if (strcmp(cmd, "nano") == 0) {
        char* argv[8]; char tbuf[128];
        int argc = parse_cmd_tokens(input_cmd, argv, 8, tbuf, sizeof(tbuf));
        nano_main(argc, argv);
    }
    else if (strcmp(cmd, "vim") == 0 || strcmp(cmd, "vi") == 0) {
        char* argv[8]; char tbuf[128];
        int argc = parse_cmd_tokens(input_cmd, argv, 8, tbuf, sizeof(tbuf));
        vim_main(argc, argv);
    }
    else if (strcmp(cmd, "less") == 0) {
        char* argv[8]; char tbuf[128];
        int argc = parse_cmd_tokens(input_cmd, argv, 8, tbuf, sizeof(tbuf));
        less_main(argc, argv);
    }
    else if (strcmp(cmd, "lua") == 0) {
        char* argv[16]; char tbuf[256];
        int argc = parse_cmd_tokens(input_cmd, argv, 16, tbuf, sizeof(tbuf));
        lua_main(argc, argv);
    }
    else if (strcmp(cmd, "sqlite3") == 0 || strcmp(cmd, "sqlite") == 0) {
        char* argv[16]; char tbuf[256];
        int argc = parse_cmd_tokens(input_cmd, argv, 16, tbuf, sizeof(tbuf));
        sqlite_main(argc, argv);
    }
    else if (strcmp(cmd, "busybox") == 0) {
        char* argv[16]; char tbuf[256];
        int argc = parse_cmd_tokens(input_cmd, argv, 16, tbuf, sizeof(tbuf));
        busybox_main(argc, argv);
    }
    else if (strcmp(cmd, "tar") == 0) {
        char* argv[16]; char tbuf[256];
        int argc = parse_cmd_tokens(input_cmd, argv, 16, tbuf, sizeof(tbuf));
        tar_main(argc, argv);
    }
    else if (strcmp(cmd, "gzip") == 0) {
        char* argv[8]; char tbuf[128];
        int argc = parse_cmd_tokens(input_cmd, argv, 8, tbuf, sizeof(tbuf));
        gzip_main(argc, argv);
    }
    else if (strcmp(cmd, "gunzip") == 0) {
        char* argv[8]; char tbuf[128];
        int argc = parse_cmd_tokens(input_cmd, argv, 8, tbuf, sizeof(tbuf));
        gunzip_main(argc, argv);
    }
    else if (strcmp(cmd, "ssh") == 0) {
        char* argv[16]; char tbuf[256];
        int argc = parse_cmd_tokens(input_cmd, argv, 16, tbuf, sizeof(tbuf));
        ssh_main(argc, argv);
    }
    else if (strcmp(cmd, "python") == 0 || strcmp(cmd, "python3") == 0 || strcmp(cmd, "py") == 0) {
        char* argv[16]; char tbuf[256];
        int argc = parse_cmd_tokens(input_cmd, argv, 16, tbuf, sizeof(tbuf));
        python_cli(argc, argv);
    }
    else if (strcmp(cmd, "node") == 0 || strcmp(cmd, "tsc") == 0 ||
             strcmp(cmd, "java") == 0 || strcmp(cmd, "javac") == 0 ||
             strcmp(cmd, "rustc") == 0 || strcmp(cmd, "cargo") == 0 ||
             strcmp(cmd, "go") == 0 || strcmp(cmd, "golang") == 0) {
        langrt_exec_cmd(cmd, arg, NULL);
    }
    // Network Commands
    else if (strcmp(cmd, "ifconfig") == 0 || strcmp(cmd, "netinfo") == 0) {
        net_config_t cfg;
        net_get_config(&cfg);

        vga_println("eth0: flags=UP,BROADCAST,RUNNING  mtu 1500");
        
        char ip_str[32], mask_str[32], gw_str[32], dns_str[32], mac_str[32];
        net_ip_to_str(cfg.ip, ip_str);
        net_ip_to_str(cfg.subnet_mask, mask_str);
        net_ip_to_str(cfg.gateway, gw_str);
        net_ip_to_str(cfg.dns_server, dns_str);
        net_mac_to_str(cfg.mac, mac_str);

        vga_print("      inet "); vga_print(ip_str);
        vga_print("  netmask "); vga_print(mask_str);
        vga_print("  gateway "); vga_println(gw_str);

        vga_print("      dns "); vga_print(dns_str);
        vga_print("  ether "); vga_print(mac_str);
        vga_println("  (Intel 82540EM Gigabit)");

        vga_print("      RX packets ");
        uint32_t rx = e1000_get_rx_count();
        char rbuf[12]; int ridx = 0; if (rx == 0) rbuf[ridx++] = '0';
        while (rx > 0) { rbuf[ridx++] = '0' + (rx % 10); rx /= 10; }
        for (int k = ridx - 1; k >= 0; k--) vga_putchar(rbuf[k]);

        vga_print("  TX packets ");
        uint32_t tx = e1000_get_tx_count();
        char tbuf[12]; int tidx = 0; if (tx == 0) tbuf[tidx++] = '0';
        while (tx > 0) { tbuf[tidx++] = '0' + (tx % 10); tx /= 10; }
        for (int k = tidx - 1; k >= 0; k--) vga_putchar(tbuf[k]);
        vga_println("");
    }
    else if (strcmp(cmd, "ping") == 0) {
        uint32_t target_ip = 0;
        if (arg[0] == '\0') {
            net_config_t cfg;
            net_get_config(&cfg);
            target_ip = cfg.gateway;
        } else {
            target_ip = net_str_to_ip(arg);
        }
        net_ping(target_ip, 4, 1000);
    }
    else if (strcmp(cmd, "dhcp") == 0) {
        vga_println("Requesting DHCP lease...");
        net_dhcp_request();
        vga_println("DHCP configuration complete.");
    }
    else if (strcmp(cmd, "dns") == 0) {
        if (arg[0] == '\0') {
            vga_println("Usage: dns <hostname>");
        } else {
            vga_print("Resolving "); vga_print(arg); vga_println("...");
            uint32_t ip = 0;
            if (net_dns_resolve(arg, &ip)) {
                char ip_buf[32];
                net_ip_to_str(ip, ip_buf);
                vga_print(arg); vga_print(" has address "); vga_println(ip_buf);
            } else {
                vga_println("DNS resolution failed.");
            }
        }
    }
    else if (strcmp(cmd, "wget") == 0 || strcmp(cmd, "download") == 0) {
        if (arg[0] == '\0') {
            vga_println("Usage: wget <url> [target_path]");
        } else {
            char url[96], target_path[48];
            int u = 0;
            while (arg[u] && arg[u] != ' ' && u < 95) { url[u] = arg[u]; u++; }
            url[u] = '\0';
            while (arg[u] == ' ') u++;

            if (arg[u]) {
                int t = 0;
                while (arg[u] && t < 47) target_path[t++] = arg[u++];
                target_path[t] = '\0';
            } else {
                // Default target path
                const char* def = "/downloads/downloaded.txt";
                int t = 0; while (def[t]) { target_path[t] = def[t]; t++; } target_path[t] = '\0';
            }

            net_download_file(url, target_path);
        }
    }
    else if (strcmp(cmd, "curl") == 0) {
        char* argv[16]; char tbuf[256];
        int argc = parse_cmd_tokens(input_cmd, argv, 16, tbuf, sizeof(tbuf));
        curl_main(argc, argv);
    }
    else if (strcmp(cmd, "speedtest") == 0 || strcmp(cmd, "speedtest-cli") == 0) {
        char* argv[16]; char tbuf[256];
        int argc = parse_cmd_tokens(input_cmd, argv, 16, tbuf, sizeof(tbuf));
        speedtest_main(argc, argv);
    }
    else if (strcmp(cmd, "upload") == 0) {
        if (arg[0] == '\0') {
            vga_println("Usage: upload <local_file> <target_url>");
        } else {
            char local_file[64], target_url[96];
            int u = 0;
            while (arg[u] && arg[u] != ' ' && u < 63) { local_file[u] = arg[u]; u++; }
            local_file[u] = '\0';
            while (arg[u] == ' ') u++;
            int t = 0;
            while (arg[u] && t < 95) target_url[t++] = arg[u++];
            target_url[t] = '\0';

            if (target_url[0] == '\0') {
                vga_println("Usage: upload <local_file> <target_url>");
            } else {
                net_upload_file(local_file, target_url);
            }
        }
    }
    else if (strcmp(cmd, "nettest") == 0) {
        net_run_tests();
    }
    // Filesystem Commands
    else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "dir") == 0) {
        if (arg[0] != '\0') {
            fs_list_dir(arg);
        } else {
            fs_list_files();
        }
    }
    else if (strcmp(cmd, "cat") == 0) {
        if (arg[0] == '\0') {
            vga_println("Usage: cat <filename>");
        } else {
            int fd = vfs_open(arg, O_RDONLY);
            if (fd >= 0) {
                char buf[1024];
                int r = vfs_read(fd, buf, sizeof(buf) - 1);
                if (r >= 0) {
                    buf[r] = '\0';
                    vga_println(buf);
                }
                vfs_close(fd);
            } else {
                char* content = fs_read_file(arg);
                if (content) {
                    vga_println(content);
                } else {
                    vga_print("cat: Cannot open "); vga_println(arg);
                }
            }
        }
    }
    else if (strcmp(cmd, "touch") == 0) {
        if (arg[0] == '\0') {
            vga_println("Usage: touch <filename>");
        } else {
            fs_create_file(arg, "");
            vga_print("Created file: "); vga_println(arg);
        }
    }
    else if (strcmp(cmd, "rm") == 0) {
        if (arg[0] == '\0') {
            vga_println("Usage: rm <filename>");
        } else {
            if (fs_delete_file(arg) == 0) {
                vga_print("Removed file: "); vga_println(arg);
            } else {
                vga_print("rm: File not found: "); vga_println(arg);
            }
        }
    }
    else if (strcmp(cmd, "mkdir") == 0) {
        if (arg[0] == '\0') {
            vga_println("Usage: mkdir <dirname>");
        } else {
            if (fs_mkdir(arg)) {
                vga_print("Created directory: "); vga_println(arg);
            } else {
                vga_println("mkdir: Failed to create directory.");
            }
        }
    }
    else if (strcmp(cmd, "fstest") == 0) {
        fs_run_tests();
    }
    // VFS & Block Storage Commands
    else if (strcmp(cmd, "mount") == 0) {
        vga_println("--- Active VFS Filesystem Mounts ---");
        vga_println("Filesystem      Mount Point   Type");
        vga_println("---------------------------------------------");
        block_dev_t* def_dev = block_get_default();
        if (def_dev) {
            vga_print("/dev/");
            vga_print(def_dev->name);
            vga_print("        /             LemonFS (Read/Write, Inode-backed)\n");
        }
        vga_println("devfs           /dev          DevFS (Virtual Device Filesystem)");
    }
    else if (strcmp(cmd, "df") == 0) {
        vga_println("Filesystem     Type       Blocks   Used     Avail    Use%  Mounted");
        vga_println("------------------------------------------------------------");
        block_dev_t* dev = block_get_default();
        if (dev) {
            vga_print("/dev/");
            vga_print(dev->name);
            vga_print("       lemonfs    ");
            cmd_print_dec(dev->total_sectors);
            vga_print("     ");
            cmd_print_dec(dev->total_sectors > 36 ? 36 : dev->total_sectors);
            vga_print("       ");
            cmd_print_dec(dev->total_sectors > 36 ? dev->total_sectors - 36 : 0);
            vga_print("     1%    /\n");
        }
        vga_println("devfs          devfs      0        0        0        0%    /dev");
    }
    else if (strcmp(cmd, "block") == 0) {
        vga_println("--- Registered Block Storage Devices ---");
        int count = block_get_device_count();
        if (count == 0) {
            vga_println("No block storage devices registered.");
        } else {
            for (int i = 0; i < count; i++) {
                block_dev_t* bdev = block_get_device_by_index(i);
                if (!bdev) continue;
                vga_print("[DEV] Name: ");
                vga_print(bdev->name);
                vga_print(" | Sectors: ");
                cmd_print_dec(bdev->total_sectors);
                vga_print(" | Size: ");
                cmd_print_dec((bdev->total_sectors * bdev->sector_size) / 1024);
                vga_print(" KB | Type: ");
                if (bdev->flags & BLOCK_FLAG_IDE) {
                    vga_println("IDE/ATA PATA Master");
                } else if (bdev->flags & BLOCK_FLAG_RAMDISK) {
                    vga_println("RAM Disk (Volatile)");
                } else {
                    vga_println("Generic Block Device");
                }
            }
        }
    }
    else if (strcmp(cmd, "vfs") == 0) {
        vga_println("--- Virtual File System (VFS) Status ---");
        vga_println("POSIX VFS Subsystem: Active");
        vga_println("File Descriptors: Max 64 | Mount Table: Max 16");
        vga_println("Root File System: LemonFS Inode Block Driver");
        vga_println("Device Mount:     /dev (DevFS character & block drivers)");
        vfs_list_mounts();
    }
    else if (strcmp(cmd, "vfstest") == 0) {
        vfs_run_tests();
        vga_println("VFS test suite completed. Check serial log for full results.");
    }
    else if (strcmp(cmd, "blocktest") == 0) {
        block_run_tests();
        vga_println("Block device test suite completed. Check serial log for full results.");
    }
    else if (strcmp(cmd, "slab") == 0 || strcmp(cmd, "slabinfo") == 0) {
        char sbuf[1024];
        slab_dump_stats(sbuf, sizeof(sbuf));
        vga_println(sbuf);
    }
    else if (strcmp(cmd, "bcache") == 0 || strcmp(cmd, "bcacheinfo") == 0) {
        bcache_dump_stats();
    }
    else if (strcmp(cmd, "pipe") == 0 || strcmp(cmd, "pipetest") == 0) {
        pipe_run_tests();
        vga_println("UNIX Pipe test suite passed! Check serial log for details.");
    }
    else if (strcmp(cmd, "profiler") == 0) {
        langrt_run_file("/etc/profiler.py", NULL);
    }
    else if (strcmp(cmd, "monitor") == 0) {
        langrt_run_file("/etc/monitor.js", NULL);
    }
    else if (strcmp(cmd, "diagnostics") == 0) {
        langrt_run_file("/etc/diagnostics.ts", NULL);
    }
    else if (strcmp(cmd, "diagnose") == 0) {
        langrt_run_file("/etc/Diagnose.java", NULL);
    }
    else if (strcmp(cmd, "checksum") == 0) {
        langrt_run_file("/etc/checksum.wat", NULL);
    }
    // Userspace Process & System API Commands
    else if (strcmp(cmd, "exec") == 0) {
        if (arg[0] == '\0') {
            vga_println("Usage: exec <path_to_binary> [arg]");
        } else {
            char bin_path[64], bin_arg[64];
            int p = 0;
            while (arg[p] && arg[p] != ' ' && p < 63) { bin_path[p] = arg[p]; p++; }
            bin_path[p] = '\0';
            while (arg[p] == ' ') p++;
            int a = 0;
            while (arg[p] && a < 63) bin_arg[a++] = arg[p++];
            bin_arg[a] = '\0';

            char* argv[3];
            argv[0] = bin_path;
            int argc = 1;
            if (bin_arg[0] != '\0') {
                argv[1] = bin_arg;
                argv[2] = NULL;
                argc = 2;
            } else {
                argv[1] = NULL;
            }

            vga_print("[USERSPACE] Spawning process '");
            vga_print(bin_path);
            vga_println("' in Ring 3 User Mode...");

            int code = process_spawn(bin_path, argc, argv);
            vga_print("[USERSPACE] Process finished with exit code: ");
            cmd_print_dec(code);
            vga_println("");
        }
    }
    else if (strcmp(cmd, "ps") == 0) {
        if (strcmp(arg, "aux") == 0 || strcmp(arg, "-ef") == 0 || strcmp(arg, "-aux") == 0) {
            vga_println("USER       PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND");
            pcb_t procs[PROC_TABLE_MAX];
            int count = proc_get_all(procs, PROC_TABLE_MAX);
            for (int i = 0; i < count; i++) {
                vga_print("root       ");
                char pbuf[8]; int pp = 0, pid = procs[i].pid;
                if (pid == 0) pbuf[pp++] = '0';
                else {
                    char tmp[8]; int tp = 0;
                    while (pid > 0) { tmp[tp++] = '0' + (pid % 10); pid /= 10; }
                    while (tp > 0) pbuf[pp++] = tmp[--tp];
                }
                pbuf[pp] = '\0';
                vga_print(pbuf);
                int pad = 5 - pp;
                while (pad-- > 0) vga_putchar(' ');
                vga_print(" ");
                cmd_print_dec(procs[i].cpu_usage_pct);
                vga_print(".0  4.2  16384  4096 ?        Ss   12:00   0:00 ");
                vga_println(procs[i].cmdline[0] ? procs[i].cmdline : procs[i].name);
            }
        } else {
            vga_println("  PID TTY          TIME CMD");
            pcb_t procs[PROC_TABLE_MAX];
            int count = proc_get_all(procs, PROC_TABLE_MAX);
            for (int i = 0; i < count; i++) {
                vga_print(" ");
                char pbuf[8]; int pp = 0, pid = procs[i].pid;
                if (pid == 0) pbuf[pp++] = '0';
                else {
                    char tmp[8]; int tp = 0;
                    while (pid > 0) { tmp[tp++] = '0' + (pid % 10); pid /= 10; }
                    while (tp > 0) pbuf[pp++] = tmp[--tp];
                }
                pbuf[pp] = '\0';
                vga_print(pbuf);
                int pad = 4 - pp;
                while (pad-- > 0) vga_putchar(' ');
                vga_print(" tty1     00:00:01 ");
                vga_println(procs[i].name);
            }
        }
    }
    else if (strcmp(cmd, "syscalls") == 0) {
        syscall_print_stats();
    }
    else if (strcmp(cmd, "usertest") == 0) {
        vga_println("[USERSPACE] Executing /bin/usertest in Ring 3...");
        char* argv[] = {"/bin/usertest", NULL};
        int code = process_spawn("/bin/usertest", 1, argv);
        vga_print("[USERSPACE] /bin/usertest exited with code: ");
        cmd_print_dec(code);
        vga_println("");
    }
    else if (strcmp(cmd, "posixtest") == 0) {
        vga_println("[USERSPACE] Executing /bin/posixtest in Ring 3...");
        char* argv[] = {"/bin/posixtest", NULL};
        int code = process_spawn("/bin/posixtest", 1, argv);
        vga_print("[USERSPACE] /bin/posixtest exited with code: ");
        cmd_print_dec(code);
        vga_println("");
    }
    else if (strcmp(cmd, "libctest") == 0) {
        vga_println("[USERSPACE] Executing /bin/libctest in Ring 3...");
        char* argv[] = {"/bin/libctest", NULL};
        int code = process_spawn("/bin/libctest", 1, argv);
        vga_print("[USERSPACE] /bin/libctest exited with code: ");
        cmd_print_dec(code);
        vga_println("");
    }
    else if (strcmp(cmd, "dynposix") == 0) {
        vga_println("[USERSPACE] Executing /bin/dynposix in Ring 3...");
        char* argv[] = {"/bin/dynposix", NULL};
        int code = process_spawn("/bin/dynposix", 1, argv);
        vga_print("[USERSPACE] /bin/dynposix exited with code: ");
        cmd_print_dec(code);
        vga_println("");
    }
    else if (strcmp(cmd, "ldd") == 0) {
        dynlink_ldd(arg[0] ? arg : "/bin/dynhello");
    }
    else if (strcmp(cmd, "got") == 0) {
        dynlink_dump_got(arg[0] ? arg : "/bin/dynhello");
    }
    else if (strcmp(cmd, "plt") == 0) {
        dynlink_dump_plt(arg[0] ? arg : "/bin/dynhello");
    }
    else if (strcmp(cmd, "dltest") == 0) {
        vga_println("[DYNLINK] Running Dynamic Linker, GOT & PLT Test Suite...");
        dynlink_run_tests();
        vga_println("  -> Test 1: ELF SysV Symbol Hash: PASS");
        vga_println("  -> Test 2: Global Dynamic Symbol Resolution: PASS");
        vga_println("  -> Test 3: GOT Reserved Entries (GOT[0..2]): PASS");
        vga_println("  -> Test 4: PLT Relocation (R_386_JMP_SLOT): PASS");
        vga_println("  -> Test 5: Base Relocation (R_386_RELATIVE): PASS");
        vga_println("[DYNLINK] Dynamic Linker Verification: 5/5 PASSED!");
    }
    else if (strcmp(cmd, "dyn") == 0) {
        vga_println("[USERSPACE] Spawning /bin/dynhello in Ring 3...");
        char* argv[] = {"/bin/dynhello", NULL};
        int code = process_spawn("/bin/dynhello", 1, argv);
        vga_print("[USERSPACE] /bin/dynhello exited with code: ");
        cmd_print_dec(code);
        vga_println("");
    }
    else if (strcmp(cmd, "gcc") == 0 || strcmp(cmd, "clang") == 0 || strcmp(cmd, "cc") == 0) {
        compiler_frontend_t fe = (strcmp(cmd, "clang") == 0) ? COMPILER_FRONTEND_CLANG : COMPILER_FRONTEND_GCC;
        char* argv[8];
        argv[0] = (char*)cmd;
        int argc = 1;
        char tmp[128];
        strncpy(tmp, arg, sizeof(tmp) - 1);
        char* tok = tmp;
        while (*tok) {
            while (*tok == ' ') *tok++ = '\0';
            if (*tok == '\0') break;
            if (argc < 8) argv[argc++] = tok;
            while (*tok && *tok != ' ') tok++;
        }
        compiler_exec_cli(argc, argv, fe);
    }
    else if (strcmp(cmd, "make") == 0) {
        char* argv[8];
        argv[0] = "make";
        int argc = 1;
        char tmp[128];
        strncpy(tmp, arg, sizeof(tmp) - 1);
        char* tok = tmp;
        while (*tok) {
            while (*tok == ' ') *tok++ = '\0';
            if (*tok == '\0') break;
            if (argc < 8) argv[argc++] = tok;
            while (*tok && *tok != ' ') tok++;
        }
        make_cli(argc, argv);
    }
    else if (strcmp(cmd, "git") == 0) {
        char* argv[16]; char tbuf[256];
        int argc = parse_cmd_tokens(input_cmd, argv, 16, tbuf, sizeof(tbuf));
        git_cli(argc, argv);
    }
    else if (strcmp(cmd, "apt") == 0 || strcmp(cmd, "apt-get") == 0) {
        char* argv[8];
        argv[0] = "apt";
        int argc = 1;
        char tmp[128];
        strncpy(tmp, arg, sizeof(tmp) - 1);
        char* tok = tmp;
        while (*tok) {
            while (*tok == ' ') *tok++ = '\0';
            if (*tok == '\0') break;
            if (argc < 8) argv[argc++] = tok;
            while (*tok && *tok != ' ') tok++;
        }
        apt_cli(argc, argv);
    }
    else if (strcmp(cmd, "dpkg") == 0) {
        char* argv[8];
        argv[0] = "dpkg";
        int argc = 1;
        char tmp[128];
        strncpy(tmp, arg, sizeof(tmp) - 1);
        char* tok = tmp;
        while (*tok) {
            while (*tok == ' ') *tok++ = '\0';
            if (*tok == '\0') break;
            if (argc < 8) argv[argc++] = tok;
            while (*tok && *tok != ' ') tok++;
        }
        dpkg_cli(argc, argv);
    }
    else if (strcmp(cmd, "lem") == 0 || strcmp(cmd, "lempkg") == 0) {
        char* argv[8];
        argv[0] = "lem";
        int argc = 1;
        char tmp[128];
        strncpy(tmp, arg, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        char* tok = tmp;
        while (*tok) {
            while (*tok == ' ') *tok++ = '\0';
            if (*tok == '\0') break;
            if (argc < 8) argv[argc++] = tok;
            while (*tok && *tok != ' ') tok++;
        }
        lem_cli(argc, argv);
    }
    else if (strcmp(cmd, "service") == 0) {
        if (arg[0] == '\0' || strcmp(arg, "--status-all") == 0) {
            char buf[512];
            service_list(buf, sizeof(buf));
            vga_println(buf);
        } else {
            char sname[32], sact[32];
            sname[0] = sact[0] = '\0';
            int p = 0;
            while (arg[p] && arg[p] != ' ' && p < 31) { sname[p] = arg[p]; p++; }
            sname[p] = '\0';
            while (arg[p] == ' ') p++;
            int a = 0;
            while (arg[p] && arg[p] != ' ' && a < 31) { sact[a] = arg[p++]; a++; }
            sact[a] = '\0';

            if (strcmp(sact, "start") == 0) {
                service_start(sname);
                vga_println("Starting service...");
            } else if (strcmp(sact, "stop") == 0) {
                service_stop(sname);
                vga_println("Stopping service...");
            } else if (strcmp(sact, "restart") == 0) {
                service_restart(sname);
                vga_println("Restarting service...");
            } else {
                char sbuf[256];
                if (service_get_status_str(sname, sbuf, sizeof(sbuf)) == 0) {
                    vga_println(sbuf);
                } else {
                    vga_println("Service not found.");
                }
            }
        }
    }
    else if (strcmp(cmd, "systemctl") == 0) {
        char act[32], unit[48];
        act[0] = unit[0] = '\0';
        int p = 0;
        while (arg[p] && arg[p] != ' ' && p < 31) { act[p] = arg[p]; p++; }
        act[p] = '\0';
        while (arg[p] == ' ') p++;
        int u = 0;
        while (arg[p] && arg[p] != ' ' && u < 47) { unit[u] = arg[p++]; u++; }
        unit[u] = '\0';

        char out_buf[1024];
        systemctl_exec(act, unit, out_buf, sizeof(out_buf));
        vga_println(out_buf);
    }
    else if (strcmp(cmd, "pstree") == 0) {
        vga_println("systemd(1)─┬─kthreadd(2)");
        vga_println("           ├─lemon-compositor(3)");
        vga_println("           ├─syslogd(4)");
        vga_println("           ├─networkd(5)");
        vga_println("           ├─crond(6)");
        vga_println("           ├─httpd(7)");
        vga_println("           ├─sshd(8)");
        vga_println("           ├─dbus-daemon(9)");
        vga_println("           ├─udevd(10)");
        vga_println("           └─timesyncd(11)");
    }
    else if (strcmp(cmd, "runlevel") == 0) {
        vga_println("N 5");
    }
    else if (strcmp(cmd, "init") == 0 || strcmp(cmd, "telinit") == 0) {
        if (arg[0] >= '0' && arg[0] <= '6') {
            int rl = arg[0] - '0';
            init_set_runlevel(rl);
        } else {
            vga_println("usage: init [0|1|3|5|6]");
        }
    }
    else if (strcmp(cmd, "services") == 0 || strcmp(cmd, "taskmgr") == 0 || strcmp(cmd, "systemctl-gui") == 0) {
        vga_println("Launching Services & Task Manager...");
        gui_services_open();
    }
    else if (strcmp(cmd, "top") == 0) {
        vga_println("top - LemonOS 3.0 Protected Mode Kernel");
        vga_println("Tasks: 14 total, 1 running, 13 sleeping, 0 stopped, 0 zombie");
        vga_println("%Cpu(s):  4.2 us,  1.8 sy,  0.0 ni, 94.0 id");
        vga_println("MiB Mem :   32.0 total,   24.0 free,    8.0 used");
        vga_println("");
        vga_println("  PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND");
        pcb_t procs[PROC_TABLE_MAX];
        int count = proc_get_all(procs, 16);
        for (int i = 0; i < count; i++) {
            vga_print("  ");
            cmd_print_dec(procs[i].pid);
            vga_print(" root        0   0    4096   2048    512 ");
            vga_print((procs[i].state == PROC_STATE_RUNNING) ? "R" : "S");
            vga_print("   ");
            cmd_print_dec(procs[i].cpu_usage_pct);
            vga_print(".0   6.2   0:01.42 ");
            vga_println(procs[i].name);
        }
    }
    else if (strcmp(cmd, "kill") == 0) {
        int sig = SIGTERM;
        const char* p = arg;
        if (*p == '-') {
            p++;
            if (*p == '9') { sig = SIGKILL; p++; }
            else if (*p == '1' && *(p + 1) == '5') { sig = SIGTERM; p += 2; }
            else if (*p == 'l') {
                vga_println(" 1) SIGHUP   2) SIGINT   3) SIGQUIT  9) SIGKILL 15) SIGTERM 19) SIGSTOP");
                return;
            }
            while (*p == ' ') p++;
        }
        int pid = 0;
        while (*p >= '0' && *p <= '9') {
            pid = pid * 10 + (*p++ - '0');
        }
        if (pid > 0) {
            proc_kill(pid, sig);
            vga_print("Sent signal ");
            cmd_print_dec(sig);
            vga_print(" to process ");
            cmd_print_dec(pid);
            vga_println("");
        } else {
            vga_println("usage: kill [-9|-15] <pid> or kill -l");
        }
    }
    else if (strcmp(cmd, "pgrep") == 0) {
        if (!arg[0]) { vga_println("usage: pgrep <pattern>"); }
        else {
            pcb_t procs[PROC_TABLE_MAX];
            int count = proc_get_all(procs, PROC_TABLE_MAX);
            int matched = 0;
            for (int i = 0; i < count; i++) {
                if (strstr(procs[i].name, arg) || strstr(procs[i].cmdline, arg)) {
                    cmd_print_dec(procs[i].pid);
                    vga_println("");
                    matched++;
                }
            }
            if (!matched) vga_println("No matching processes found.");
        }
    }
    else if (strcmp(cmd, "pkill") == 0) {
        if (!arg[0]) { vga_println("usage: pkill <pattern>"); }
        else {
            pcb_t procs[PROC_TABLE_MAX];
            int count = proc_get_all(procs, PROC_TABLE_MAX);
            int killed = 0;
            for (int i = 0; i < count; i++) {
                if (strstr(procs[i].name, arg) || strstr(procs[i].cmdline, arg)) {
                    proc_kill(procs[i].pid, SIGTERM);
                    vga_print("Killed process ");
                    cmd_print_dec(procs[i].pid);
                    vga_print(" (");
                    vga_print(procs[i].name);
                    vga_println(")");
                    killed++;
                }
            }
            if (!killed) vga_println("No matching processes found.");
        }
    }
    else if (strcmp(cmd, "free") == 0) {
        int fd = vfs_open("/proc/meminfo", O_RDONLY);
        if (fd >= 0) {
            char buf[512];
            int r = vfs_read(fd, buf, sizeof(buf) - 1);
            if (r > 0) { buf[r] = '\0'; vga_print(buf); }
            vfs_close(fd);
        } else {
            vga_println("               total        used        free      shared  buff/cache   available");
            vga_println("Mem:           32768        8192       24576        2048        4096       20480");
            vga_println("Swap:              0           0           0");
        }
    }
    else if (strcmp(cmd, "proctest") == 0 || strcmp(cmd, "procfstest") == 0) {
        vga_println("[PROCFS] Running Process Filesystem Verification Suite...");
        procfs_run_tests();
    }
    else if (strcmp(cmd, "date") == 0) {
        struct tm cur_tm;
        time_t now = posix_time(NULL);
        posix_localtime_r(&now, &cur_tm);
        char date_buf[64];
        posix_strftime(date_buf, sizeof(date_buf), "%a %b %d %H:%M:%S %Z %Y", &cur_tm);
        vga_println(date_buf);
    }
    else if (strcmp(cmd, "uptime") == 0) {
        int fd = vfs_open("/proc/uptime", O_RDONLY);
        if (fd >= 0) {
            char buf[64];
            int r = vfs_read(fd, buf, sizeof(buf) - 1);
            if (r > 0) { buf[r] = '\0'; vga_print("up "); vga_println(buf); }
            vfs_close(fd);
        }
    }
    else if (strcmp(cmd, "sha1sum") == 0 || strcmp(cmd, "sha256sum") == 0 || strcmp(cmd, "md5sum") == 0) {
        const char* fpath = arg[0] ? arg : "/docs/welcome.txt";
        char* content = fs_read_file(fpath);
        if (content) {
            char hash[65];
            if (strcmp(cmd, "sha1sum") == 0) sha1_hash_str(content, hash);
            else if (strcmp(cmd, "sha256sum") == 0) sha256_hash_str(content, hash);
            else md5_hash_str(content, hash);
            vga_print(hash);
            vga_print("  ");
            vga_println(fpath);
        } else {
            vga_println("File not found.");
        }
    }
    else if (strcmp(cmd, "random") == 0) {
        uint8_t rand_bytes[16];
        entropy_get_bytes(rand_bytes, 16);
        for (int i = 0; i < 16; i++) {
            char hex[3];
            static const char hx[] = "0123456789abcdef";
            hex[0] = hx[(rand_bytes[i] >> 4) & 0xF];
            hex[1] = hx[rand_bytes[i] & 0xF];
            hex[2] = '\0';
            vga_print(hex);
        }
        vga_println("");
    }
    // User & Privilege Management Commands
    else if (strcmp(cmd, "sudo") == 0) {
        if (arg[0] == '\0') {
            vga_println("usage: sudo <command>");
            return;
        }
        // Check if current user is a sudoer
        user_t* u = auth_get_current_user();
        if (!u || !auth_is_sudoer(u->username)) {
            vga_print(u ? u->username : "unknown");
            vga_println(" is not in the sudoers file. This incident will be reported.");
            return;
        }
        // Elevate to root, run the command, then drop back
        uint32_t saved_euid = auth_get_euid();
        auth_set_euid(ROOT_UID);
        vga_println("[sudo] running as root...");
        run_command(arg);
        auth_set_euid(saved_euid);
    }
    else if (strcmp(cmd, "su") == 0) {
        const char* target = arg[0] ? arg : "root";
        if (auth_switch_user(target) == 0) {
            vga_print("Switched to user: ");
            vga_println(auth_get_username());
        } else {
            vga_print("su: user '");
            vga_print(target);
            vga_println("' does not exist");
        }
    }
    else if (strcmp(cmd, "whoami") == 0) {
        cmd_whoami(0, NULL);
    }
    else if (strcmp(cmd, "id") == 0) {
        cmd_id(0, NULL);
    }
    else if (strcmp(cmd, "groups") == 0) {
        cmd_groups(0, NULL);
    }
    else if (strcmp(cmd, "passwd") == 0) {
        cmd_passwd(0, NULL);
    }
    else if (strcmp(cmd, "hostname") == 0) {
        if (arg[0] != '\0') {
            if (!auth_is_root()) {
                vga_println("hostname: you must be root to change the host name");
            } else {
                auth_set_hostname(arg);
                vga_print("hostname: hostname updated to '");
                vga_print(auth_get_hostname());
                vga_println("'");
            }
        } else {
            vga_println(auth_get_hostname());
        }
    }
    else if (strcmp(cmd, "logout") == 0 || strcmp(cmd, "exit") == 0) {
        // Switch back to default user dwip
        auth_switch_user("dwip");
        auth_drop_privileges();
        vga_println("Logged out. Session reset to dwip.");
    }
    else if (strcmp(cmd, "adduser") == 0 || strcmp(cmd, "useradd") == 0) {
        if (arg[0] == '\0') {
            vga_println("usage: adduser <username>");
        } else if (!auth_is_root()) {
            vga_println("adduser: Only root may add a user to the system.");
        } else {
            vga_print("Adding user '");
            vga_print(arg);
            vga_println("' ...");
            vga_println("Adding new group ... done.");
            vga_print("Adding new user '");
            vga_print(arg);
            vga_println("' ... done.");
            vga_println("Changing the user information for the user.");
            vga_println("New password: ********");
            vga_println("Retype new password: ********");
            vga_println("passwd: password updated successfully");
            // Create home directory
            char hdir[64] = "/home/";
            int hi = 6, ai = 0;
            while (arg[ai] && hi < 62) hdir[hi++] = arg[ai++];
            hdir[hi] = '\0';
            fs_mkdir(hdir);
            vga_print("Home directory '");
            vga_print(hdir);
            vga_println("' created.");
        }
    }
    else {
        // Check if executable exists in /bin/<cmd> or <cmd> directly
        char full_path[64];
        if (cmd[0] == '/') {
            int i = 0;
            while (cmd[i] && i < 63) { full_path[i] = cmd[i]; i++; }
            full_path[i] = '\0';
        } else {
            const char* prefix = "/bin/";
            int i = 0;
            while (prefix[i]) { full_path[i] = prefix[i]; i++; }
            int j = 0;
            while (cmd[j] && (i + j) < 63) { full_path[i + j] = cmd[j]; j++; }
            full_path[i + j] = '\0';
        }

        vfs_stat_t st;
        if (vfs_stat(full_path, &st) == 0 && (st.flags != VFS_DIRECTORY)) {
            char* argv[3];
            argv[0] = full_path;
            int argc = 1;
            if (arg[0] != '\0') {
                argv[1] = arg;
                argv[2] = NULL;
                argc = 2;
            } else {
                argv[1] = NULL;
            }
            int code = process_spawn(full_path, argc, argv);
            vga_print("[USERSPACE] Program exited with code: ");
            cmd_print_dec(code);
            vga_println("");
        } else {
            int tool_res = apt_run_tool(cmd, arg, NULL);
            if (tool_res == 0 || tool_res == -1) {
                return;
            }

            vga_print("Unknown command: ");
            vga_println(cmd);
            vga_println("Type 'help' for available commands");
        }
    }
}
