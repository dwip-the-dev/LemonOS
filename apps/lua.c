// Lua 5.4 Lightweight Runtime & Interactive REPL for LemonOS
// Author: Dwip (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "lua.h"
#include "vga.h"
#include "keyboard.h"
#include "filesystem.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "math.h"

typedef enum {
    LUA_T_NIL,
    LUA_T_NUMBER,
    LUA_T_STRING,
    LUA_T_BOOLEAN,
    LUA_T_TABLE
} lua_type_t;

typedef struct lua_val {
    lua_type_t type;
    union {
        double num;
        char str[128];
        int boolean;
    } val;
} lua_val_t;

typedef struct lua_var {
    char name[32];
    lua_val_t v;
    int is_used;
} lua_var_t;

#define MAX_LUA_VARS 128
static lua_var_t global_env[MAX_LUA_VARS];
static int env_init_done = 0;

static void lua_env_init(void) {
    if (env_init_done) return;
    memset(global_env, 0, sizeof(global_env));
    env_init_done = 1;
}

static lua_val_t lua_get_var(const char* name) {
    for (int i = 0; i < MAX_LUA_VARS; i++) {
        if (global_env[i].is_used && strcmp(global_env[i].name, name) == 0) {
            return global_env[i].v;
        }
    }
    // Constants
    if (strcmp(name, "math.pi") == 0) {
        lua_val_t res; res.type = LUA_T_NUMBER; res.val.num = 3.141592653589793; return res;
    }
    if (strcmp(name, "_VERSION") == 0) {
        lua_val_t res; res.type = LUA_T_STRING; strcpy(res.val.str, "Lua 5.4.6"); return res;
    }
    if (strcmp(name, "_OS") == 0) {
        lua_val_t res; res.type = LUA_T_STRING; strcpy(res.val.str, "LemonOS"); return res;
    }
    lua_val_t nil_val; nil_val.type = LUA_T_NIL;
    return nil_val;
}

static void lua_set_var(const char* name, lua_val_t v) {
    for (int i = 0; i < MAX_LUA_VARS; i++) {
        if (global_env[i].is_used && strcmp(global_env[i].name, name) == 0) {
            global_env[i].v = v;
            return;
        }
    }
    for (int i = 0; i < MAX_LUA_VARS; i++) {
        if (!global_env[i].is_used) {
            strncpy(global_env[i].name, name, sizeof(global_env[i].name) - 1);
            global_env[i].v = v;
            global_env[i].is_used = 1;
            return;
        }
    }
}

// Simple expression evaluator (handles numbers, strings, vars, arithmetic +, -, *, /, %, ^)
static lua_val_t lua_eval_expr(const char* expr) {
    lua_val_t res;
    res.type = LUA_T_NIL;
    if (!expr) return res;

    // Skip leading spaces
    while (*expr == ' ') expr++;
    if (!*expr) return res;

    // String literal "..." or '...'
    if (*expr == '"' || *expr == '\'') {
        char quote = *expr++;
        int idx = 0;
        while (*expr && *expr != quote && idx < 127) {
            res.val.str[idx++] = *expr++;
        }
        res.val.str[idx] = '\0';
        res.type = LUA_T_STRING;
        return res;
    }

    // Boolean literals
    if (strncmp(expr, "true", 4) == 0 && (expr[4] == ' ' || expr[4] == '\0' || expr[4] == ')')) {
        res.type = LUA_T_BOOLEAN; res.val.boolean = 1; return res;
    }
    if (strncmp(expr, "false", 5) == 0 && (expr[5] == ' ' || expr[5] == '\0' || expr[5] == ')')) {
        res.type = LUA_T_BOOLEAN; res.val.boolean = 0; return res;
    }
    if (strncmp(expr, "nil", 3) == 0 && (expr[3] == ' ' || expr[3] == '\0' || expr[3] == ')')) {
        res.type = LUA_T_NIL; return res;
    }

    // Math functions
    if (strncmp(expr, "math.sqrt(", 10) == 0) {
        lua_val_t arg = lua_eval_expr(expr + 10);
        res.type = LUA_T_NUMBER;
        res.val.num = (arg.type == LUA_T_NUMBER && arg.val.num >= 0) ? sqrt(arg.val.num) : 0;
        return res;
    }
    if (strncmp(expr, "math.abs(", 9) == 0) {
        lua_val_t arg = lua_eval_expr(expr + 9);
        res.type = LUA_T_NUMBER;
        res.val.num = (arg.type == LUA_T_NUMBER && arg.val.num < 0) ? -arg.val.num : (arg.type == LUA_T_NUMBER ? arg.val.num : 0);
        return res;
    }
    if (strncmp(expr, "math.floor(", 11) == 0) {
        lua_val_t arg = lua_eval_expr(expr + 11);
        res.type = LUA_T_NUMBER;
        res.val.num = (int)arg.val.num;
        return res;
    }
    if (strncmp(expr, "string.len(", 11) == 0) {
        lua_val_t arg = lua_eval_expr(expr + 11);
        res.type = LUA_T_NUMBER;
        res.val.num = (arg.type == LUA_T_STRING) ? strlen(arg.val.str) : 0;
        return res;
    }

    // Arithmetic / concatenation parser
    // Check for binary operations: +, -, *, /, %, ..
    const char* ops[] = {"..", "+", "-", "*", "/", "%", "^", "==", "!=", "<=", ">=", "<", ">"};
    for (size_t o = 0; o < sizeof(ops)/sizeof(ops[0]); o++) {
        const char* p = strstr(expr, ops[o]);
        if (p && p != expr) {
            char left[64]; char right[64];
            int llen = p - expr;
            if (llen > 63) llen = 63;
            strncpy(left, expr, llen); left[llen] = '\0';
            strncpy(right, p + strlen(ops[o]), sizeof(right) - 1); right[sizeof(right) - 1] = '\0';

            lua_val_t v1 = lua_eval_expr(left);
            lua_val_t v2 = lua_eval_expr(right);

            if (strcmp(ops[o], "..") == 0) {
                // String concat
                char buf1[128]; char buf2[128];
                if (v1.type == LUA_T_NUMBER) snprintf(buf1, sizeof(buf1), "%g", v1.val.num);
                else strncpy(buf1, v1.val.str, sizeof(buf1) - 1);

                if (v2.type == LUA_T_NUMBER) snprintf(buf2, sizeof(buf2), "%g", v2.val.num);
                else strncpy(buf2, v2.val.str, sizeof(buf2) - 1);

                res.type = LUA_T_STRING;
                snprintf(res.val.str, sizeof(res.val.str), "%s%s", buf1, buf2);
                return res;
            }

            if (v1.type == LUA_T_NUMBER && v2.type == LUA_T_NUMBER) {
                res.type = LUA_T_NUMBER;
                if (strcmp(ops[o], "+") == 0) res.val.num = v1.val.num + v2.val.num;
                else if (strcmp(ops[o], "-") == 0) res.val.num = v1.val.num - v2.val.num;
                else if (strcmp(ops[o], "*") == 0) res.val.num = v1.val.num * v2.val.num;
                else if (strcmp(ops[o], "/") == 0) res.val.num = (v2.val.num != 0) ? (v1.val.num / v2.val.num) : 0;
                else if (strcmp(ops[o], "%") == 0) res.val.num = (int)v1.val.num % ((int)v2.val.num ? (int)v2.val.num : 1);
                else if (strcmp(ops[o], "==") == 0) { res.type = LUA_T_BOOLEAN; res.val.boolean = (v1.val.num == v2.val.num); }
                else if (strcmp(ops[o], "<=") == 0) { res.type = LUA_T_BOOLEAN; res.val.boolean = (v1.val.num <= v2.val.num); }
                else if (strcmp(ops[o], ">=") == 0) { res.type = LUA_T_BOOLEAN; res.val.boolean = (v1.val.num >= v2.val.num); }
                else if (strcmp(ops[o], "<") == 0) { res.type = LUA_T_BOOLEAN; res.val.boolean = (v1.val.num < v2.val.num); }
                else if (strcmp(ops[o], ">") == 0) { res.type = LUA_T_BOOLEAN; res.val.boolean = (v1.val.num > v2.val.num); }
                return res;
            }
        }
    }

    // Single number
    if ((*expr >= '0' && *expr <= '9') || (*expr == '-' && expr[1] >= '0' && expr[1] <= '9')) {
        res.type = LUA_T_NUMBER;
        res.val.num = atof(expr);
        return res;
    }

    // Variable lookup
    char vname[32];
    int vlen = 0;
    while (*expr && *expr != ' ' && *expr != ')' && *expr != ',' && vlen < 31) {
        vname[vlen++] = *expr++;
    }
    vname[vlen] = '\0';
    return lua_get_var(vname);
}

// Print helper
static void lua_print_val(lua_val_t v) {
    if (v.type == LUA_T_NIL) {
        printf("nil");
    } else if (v.type == LUA_T_NUMBER) {
        if ((double)(long)v.val.num == v.val.num) printf("%ld", (long)v.val.num);
        else printf("%f", v.val.num);
    } else if (v.type == LUA_T_STRING) {
        printf("%s", v.val.str);
    } else if (v.type == LUA_T_BOOLEAN) {
        printf("%s", v.val.boolean ? "true" : "false");
    } else if (v.type == LUA_T_TABLE) {
        printf("table: 0x%08x", (unsigned int)&v);
    }
}

int lua_eval_string(const char* line) {
    lua_env_init();
    if (!line) return 0;
    while (*line == ' ') line++;
    if (!*line || *line == '#') return 0;

    // Comment
    if (strncmp(line, "--", 2) == 0) return 0;

    // print(...)
    if (strncmp(line, "print(", 6) == 0) {
        const char* p = line + 6;
        char arg_buf[128];
        int idx = 0;
        int in_quote = 0;
        while (*p && (*p != ')' || in_quote)) {
            if (*p == '"' || *p == '\'') in_quote = !in_quote;
            if (*p == ',' && !in_quote) {
                arg_buf[idx] = '\0';
                lua_print_val(lua_eval_expr(arg_buf));
                printf("\t");
                idx = 0;
                p++;
                while (*p == ' ') p++;
                continue;
            }
            if (idx < 127) arg_buf[idx++] = *p;
            p++;
        }
        arg_buf[idx] = '\0';
        lua_print_val(lua_eval_expr(arg_buf));
        printf("\n");
        return 0;
    }

    // Variable assignment: name = expr
    const char* eq = strchr(line, '=');
    if (eq && eq != line && *(eq + 1) != '=') {
        char vname[32];
        int nlen = eq - line;
        while (nlen > 0 && line[nlen - 1] == ' ') nlen--;
        if (nlen > 31) nlen = 31;
        strncpy(vname, line, nlen); vname[nlen] = '\0';

        const char* val_str = eq + 1;
        while (*val_str == ' ') val_str++;
        lua_val_t val = lua_eval_expr(val_str);
        lua_set_var(vname, val);
        return 0;
    }

    // Direct expression evaluation (e.g. `10 + 20` or `math.pi`)
    lua_val_t direct = lua_eval_expr(line);
    if (direct.type != LUA_T_NIL) {
        lua_print_val(direct);
        printf("\n");
    }

    return 0;
}

int lua_run_file(const char* filename) {
    char* content = fs_read_file(filename);
    if (!content) {
        printf("lua: cannot open %s: No such file or directory\n", filename);
        return 1;
    }
    char line[256];
    const char* p = content;
    while (*p) {
        int idx = 0;
        while (*p && *p != '\n' && *p != '\r' && idx < 255) {
            line[idx++] = *p++;
        }
        line[idx] = '\0';
        if (*p == '\r') p++;
        if (*p == '\n') p++;
        lua_eval_string(line);
    }
    return 0;
}

int lua_main(int argc, char** argv) {
    lua_env_init();

    if (argc > 1) {
        if (strcmp(argv[1], "-e") == 0 && argc > 2) {
            return lua_eval_string(argv[2]);
        }
        if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
            printf("Lua 5.4.6 Copyright (C) 1994-2023 Lua.org, PUC-Rio (LemonOS Edition)\n");
            return 0;
        }
        return lua_run_file(argv[1]);
    }

    // Interactive REPL
    printf("Lua 5.4.6  Copyright (C) 1994-2023 Lua.org, PUC-Rio\n");
    printf("LemonOS Baremetal Embedded Runtime. Type 'quit' or 'os.exit()' to exit.\n\n");

    char buf[128];
    while (1) {
        printf("> ");
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

        if (strcmp(buf, "quit") == 0 || strcmp(buf, "os.exit()") == 0 || strcmp(buf, "exit") == 0) {
            break;
        }

        if (buf[0]) {
            lua_eval_string(buf);
        }
    }

    return 0;
}
