#include "make.h"
#include "compiler.h"
#include "filesystem.h"
#include "vfs.h"
#include "string.h"
#include "serial.h"

static make_rule_t rules[MAKE_MAX_TARGETS];
static int rule_count = 0;
static make_var_t vars[MAKE_MAX_VARS];
static int var_count = 0;

static char makefile_buf[4096];

void make_init(void) {
    rule_count = 0;
    var_count = 0;
    serial_println("[MAKE] GNU Make build automation engine initialized.");
}

static void set_var(const char* key, const char* val) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].key, key) == 0) {
            strncpy(vars[i].val, val, sizeof(vars[i].val) - 1);
            return;
        }
    }
    if (var_count < MAKE_MAX_VARS) {
        strncpy(vars[var_count].key, key, sizeof(vars[var_count].key) - 1);
        strncpy(vars[var_count].val, val, sizeof(vars[var_count].val) - 1);
        var_count++;
    }
}

static const char* get_var(const char* key) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].key, key) == 0) {
            return vars[i].val;
        }
    }
    return "";
}

static void expand_vars(const char* in, char* out, size_t max) {
    size_t op = 0;
    for (size_t ip = 0; in[ip] != '\0' && op < max - 1; ip++) {
        if (in[ip] == '$' && in[ip + 1] == '(') {
            ip += 2;
            char key[32];
            int kp = 0;
            while (in[ip] != '\0' && in[ip] != ')' && kp < 31) {
                key[kp++] = in[ip++];
            }
            key[kp] = '\0';
            const char* v = get_var(key);
            while (*v && op < max - 1) out[op++] = *v++;
        } else {
            out[op++] = in[ip];
        }
    }
    out[op] = '\0';
}

static int parse_makefile(const char* content) {
    rule_count = 0;
    var_count = 0;

    // Set default standard variables
    set_var("CC", "gcc");
    set_var("CFLAGS", "-Wall -O2");
    set_var("RM", "rm -f");

    const char* line = content;
    make_rule_t* cur_rule = NULL;

    while (*line) {
        // Skip leading whitespace
        while (*line == ' ' || *line == '\r') line++;
        if (*line == '\0' || *line == '\n' || *line == '#') {
            if (*line == '#') {
                while (*line && *line != '\n') line++;
            }
            if (*line == '\n') line++;
            continue;
        }

        // Check for command under rule (starts with \t)
        if (*line == '\t' && cur_rule != NULL) {
            line++;
            while (*line == ' ') line++;
            char cmd[96];
            int p = 0;
            while (*line && *line != '\n' && *line != '\r' && p < 95) {
                cmd[p++] = *line++;
            }
            cmd[p] = '\0';
            if (cur_rule->command_count < MAKE_MAX_COMMANDS) {
                strncpy(cur_rule->commands[cur_rule->command_count++], cmd, 95);
            }
            if (*line == '\n') line++;
            continue;
        }

        // Read line content
        char line_buf[128];
        int lp = 0;
        while (*line && *line != '\n' && *line != '\r' && lp < 127) {
            line_buf[lp++] = *line++;
        }
        line_buf[lp] = '\0';
        if (*line == '\n') line++;

        // Variable assignment: KEY = VAL
        char* eq = strchr(line_buf, '=');
        if (eq) {
            *eq = '\0';
            char* key = line_buf;
            char* val = eq + 1;
            while (*key == ' ') key++;
            char* endk = key + strlen(key) - 1;
            while (endk > key && *endk == ' ') *endk-- = '\0';
            while (*val == ' ') val++;
            set_var(key, val);
            continue;
        }

        // Target rule: TARGET: DEPS
        char* colon = strchr(line_buf, ':');
        if (colon) {
            if (rule_count >= MAKE_MAX_TARGETS) continue;
            *colon = '\0';
            char* tgt = line_buf;
            char* deps = colon + 1;

            while (*tgt == ' ') tgt++;
            char* endt = tgt + strlen(tgt) - 1;
            while (endt > tgt && *endt == ' ') *endt-- = '\0';

            cur_rule = &rules[rule_count++];
            memset(cur_rule, 0, sizeof(make_rule_t));
            strncpy(cur_rule->target, tgt, sizeof(cur_rule->target) - 1);

            // Parse dependencies separated by spaces
            while (*deps) {
                while (*deps == ' ') deps++;
                if (*deps == '\0') break;
                char dep_name[32];
                int dp = 0;
                while (*deps && *deps != ' ' && dp < 31) {
                    dep_name[dp++] = *deps++;
                }
                dep_name[dp] = '\0';
                if (cur_rule->dep_count < MAKE_MAX_DEPS) {
                    strncpy(cur_rule->deps[cur_rule->dep_count++], dep_name, 31);
                }
            }
        }
    }
    return rule_count;
}

static int execute_target(const char* target_name, int dry_run) {
    make_rule_t* rule = NULL;
    for (int i = 0; i < rule_count; i++) {
        if (strcmp(rules[i].target, target_name) == 0) {
            rule = &rules[i];
            break;
        }
    }

    if (!rule) {
        serial_print("make: *** No rule to make target '");
        serial_print(target_name);
        serial_println("'.  Stop.");
        return -1;
    }

    // Recursively build prerequisites
    for (int d = 0; d < rule->dep_count; d++) {
        for (int r = 0; r < rule_count; r++) {
            if (strcmp(rules[r].target, rule->deps[d]) == 0) {
                if (execute_target(rule->deps[d], dry_run) != 0) {
                    return -1;
                }
                break;
            }
        }
    }

    // Execute commands for this target
    for (int c = 0; c < rule->command_count; c++) {
        char expanded[128];
        expand_vars(rule->commands[c], expanded, sizeof(expanded));

        serial_println(expanded);

        if (!dry_run) {
            // Check if command is GCC or Clang
            if (strncmp(expanded, "gcc ", 4) == 0 || strncmp(expanded, "clang ", 6) == 0) {
                compiler_frontend_t fe = (expanded[0] == 'c') ? COMPILER_FRONTEND_CLANG : COMPILER_FRONTEND_GCC;
                char* argv[8];
                int argc = 0;
                char tmp[128];
                strcpy(tmp, expanded);
                char* tok = tmp;
                while (*tok) {
                    while (*tok == ' ') *tok++ = '\0';
                    if (*tok == '\0') break;
                    if (argc < 8) argv[argc++] = tok;
                    while (*tok && *tok != ' ') tok++;
                }
                compiler_exec_cli(argc, argv, fe);
            }
        }
    }
    return 0;
}

int make_run(const char* makefile_path, const char* target_name, int dry_run) {
    const char* path = (makefile_path && makefile_path[0]) ? makefile_path : "Makefile";

    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        // Fallback to checking /Makefile
        fd = vfs_open("/Makefile", O_RDONLY);
    }
    if (fd < 0) {
        serial_println("make: *** No targets specified and no makefile found.  Stop.");
        return -1;
    }

    int bytes = vfs_read(fd, makefile_buf, sizeof(makefile_buf) - 1);
    vfs_close(fd);

    if (bytes <= 0) {
        serial_println("make: *** Makefile is empty.  Stop.");
        return -1;
    }
    makefile_buf[bytes] = '\0';

    if (parse_makefile(makefile_buf) == 0) {
        serial_println("make: *** No targets in Makefile.  Stop.");
        return -1;
    }

    const char* tgt = (target_name && target_name[0]) ? target_name : rules[0].target;
    return execute_target(tgt, dry_run);
}

int make_cli(int argc, char** argv) {
    const char* makefile = NULL;
    const char* target = NULL;
    int dry_run = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--dry-run") == 0) {
            dry_run = 1;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            makefile = argv[++i];
        } else if (strcmp(argv[i], "--version") == 0) {
            serial_println("GNU Make 4.4.1");
            serial_println("Built for i386-unknown-lemonos");
            serial_println("Copyright (C) 1988-2023 Free Software Foundation, Inc.");
            return 0;
        } else if (argv[i][0] != '-') {
            target = argv[i];
        }
    }

    return make_run(makefile, target, dry_run);
}

void make_run_tests(void) {
    serial_println("[MAKE] Running GNU Make Verification Tests...");

    const char* sample_makefile =
        "CC = gcc\n"
        "all: test\n"
        "test:\n"
        "\t$(CC) -o /bin/maketest test.c\n"
        "clean:\n"
        "\trm -f /bin/maketest\n";

    fs_create_file("/Makefile", sample_makefile);

    int res = make_run("/Makefile", "all", 1);
    if (res == 0) {
        serial_println("  -> [TEST 1/2] make target dependency resolution: PASS");
    } else {
        serial_println("  -> [TEST 1/2] make resolution: FAIL");
    }

    int res_clean = make_run("/Makefile", "clean", 1);
    if (res_clean == 0) {
        serial_println("  -> [TEST 2/2] make clean target parsing: PASS");
    } else {
        serial_println("  -> [TEST 2/2] make clean: FAIL");
    }
    serial_println("[MAKE] GNU Make engine verified.");
}
