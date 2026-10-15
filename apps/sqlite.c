// SQLite 3.44 Embedded Relational Database Engine for LemonOS
// Author: Dwip (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "sqlite.h"
#include "vga.h"
#include "keyboard.h"
#include "filesystem.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

#define MAX_TABLES 16
#define MAX_COLS 8
#define MAX_ROWS 64
#define MAX_STR_LEN 32

typedef struct {
    char name[MAX_STR_LEN];
} col_def_t;

typedef struct {
    char values[MAX_COLS][MAX_STR_LEN];
} row_t;

typedef struct {
    char name[MAX_STR_LEN];
    col_def_t cols[MAX_COLS];
    int num_cols;
    row_t rows[MAX_ROWS];
    int num_rows;
    int is_used;
} table_t;

typedef struct {
    table_t tables[MAX_TABLES];
    int num_tables;
    char db_name[64];
} db_t;

static db_t current_db;

static void db_init(const char* name) {
    memset(&current_db, 0, sizeof(current_db));
    strncpy(current_db.db_name, name ? name : ":memory:", sizeof(current_db.db_name) - 1);
}

// Persist database state to LemonFS
static void db_save(void) {
    if (strcmp(current_db.db_name, ":memory:") == 0) return;
    char buf[2048];
    int len = 0;
    buf[0] = '\0';

    for (int t = 0; t < MAX_TABLES; t++) {
        if (!current_db.tables[t].is_used) continue;
        table_t* tbl = &current_db.tables[t];
        len += snprintf(buf + len, sizeof(buf) - len, "CREATE TABLE %s (", tbl->name);
        for (int c = 0; c < tbl->num_cols; c++) {
            len += snprintf(buf + len, sizeof(buf) - len, "%s%s", tbl->cols[c].name, (c < tbl->num_cols - 1) ? ", " : ");\n");
        }
        for (int r = 0; r < tbl->num_rows; r++) {
            len += snprintf(buf + len, sizeof(buf) - len, "INSERT INTO %s VALUES (", tbl->name);
            for (int c = 0; c < tbl->num_cols; c++) {
                len += snprintf(buf + len, sizeof(buf) - len, "'%s'%s", tbl->rows[r].values[c], (c < tbl->num_cols - 1) ? ", " : ");\n");
            }
        }
    }
    fs_create_file(current_db.db_name, buf);
}

static void db_load(const char* filename) {
    db_init(filename);
    char* content = fs_read_file(filename);
    if (!content) return;

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
        if (line[0]) sqlite_exec(filename, line);
    }
}

int sqlite_exec(const char* db_file, const char* sql) {
    (void)db_file;
    if (!sql) return -1;
    while (*sql == ' ') sql++;
    if (!*sql) return 0;

    // Dot commands
    if (*sql == '.') {
        if (strcmp(sql, ".tables") == 0) {
            for (int i = 0; i < MAX_TABLES; i++) {
                if (current_db.tables[i].is_used) {
                    printf("%s  ", current_db.tables[i].name);
                }
            }
            printf("\n");
            return 0;
        } else if (strncmp(sql, ".schema", 7) == 0) {
            for (int i = 0; i < MAX_TABLES; i++) {
                if (current_db.tables[i].is_used) {
                    table_t* t = &current_db.tables[i];
                    printf("CREATE TABLE %s (", t->name);
                    for (int c = 0; c < t->num_cols; c++) {
                        printf("%s%s", t->cols[c].name, (c < t->num_cols - 1) ? ", " : ");\n");
                    }
                }
            }
            return 0;
        } else if (strcmp(sql, ".dump") == 0) {
            printf("PRAGMA foreign_keys=OFF;\nBEGIN TRANSACTION;\n");
            for (int t = 0; t < MAX_TABLES; t++) {
                if (!current_db.tables[t].is_used) continue;
                table_t* tbl = &current_db.tables[t];
                printf("CREATE TABLE %s (", tbl->name);
                for (int c = 0; c < tbl->num_cols; c++) {
                    printf("%s%s", tbl->cols[c].name, (c < tbl->num_cols - 1) ? ", " : ");\n");
                }
                for (int r = 0; r < tbl->num_rows; r++) {
                    printf("INSERT INTO %s VALUES (", tbl->name);
                    for (int c = 0; c < tbl->num_cols; c++) {
                        printf("'%s'%s", tbl->rows[r].values[c], (c < tbl->num_cols - 1) ? ", " : ");\n");
                    }
                }
            }
            printf("COMMIT;\n");
            return 0;
        } else if (strcmp(sql, ".help") == 0) {
            printf(".dump         Render all database content as SQL\n");
            printf(".help         Show this message\n");
            printf(".schema ?TAB? Show the CREATE statements\n");
            printf(".tables       List names of tables\n");
            printf(".quit         Exit this program\n");
            return 0;
        }
        printf("Error: unknown command or invalid arguments: \"%s\". Enter \".help\" for help\n", sql);
        return 0;
    }

    // CREATE TABLE name (col1, col2, ...)
    if (strncasecmp(sql, "CREATE TABLE", 12) == 0) {
        const char* p = sql + 12;
        while (*p == ' ') p++;
        char tname[MAX_STR_LEN];
        int tidx = 0;
        while (*p && *p != ' ' && *p != '(' && tidx < MAX_STR_LEN - 1) {
            tname[tidx++] = *p++;
        }
        tname[tidx] = '\0';

        // Check if table exists
        int slot = -1;
        for (int i = 0; i < MAX_TABLES; i++) {
            if (current_db.tables[i].is_used && strcmp(current_db.tables[i].name, tname) == 0) {
                printf("Error: table %s already exists\n", tname);
                return -1;
            }
            if (!current_db.tables[i].is_used && slot == -1) slot = i;
        }
        if (slot == -1) {
            printf("Error: maximum tables limit reached\n");
            return -1;
        }

        table_t* tbl = &current_db.tables[slot];
        memset(tbl, 0, sizeof(table_t));
        strncpy(tbl->name, tname, sizeof(tbl->name) - 1);
        tbl->is_used = 1;

        // Parse column names inside parentheses
        const char* paren = strchr(p, '(');
        if (paren) {
            paren++;
            while (*paren && *paren != ')' && tbl->num_cols < MAX_COLS) {
                while (*paren == ' ' || *paren == ',') paren++;
                char cname[MAX_STR_LEN];
                int cidx = 0;
                while (*paren && *paren != ' ' && *paren != ',' && *paren != ')' && cidx < MAX_STR_LEN - 1) {
                    cname[cidx++] = *paren++;
                }
                cname[cidx] = '\0';
                if (cname[0]) {
                    strncpy(tbl->cols[tbl->num_cols++].name, cname, MAX_STR_LEN - 1);
                }
                // Skip optional type definition (e.g. TEXT, INTEGER PRIMARY KEY)
                while (*paren && *paren != ',' && *paren != ')') paren++;
            }
        }
        db_save();
        return 0;
    }

    // INSERT INTO name VALUES (val1, val2, ...)
    if (strncasecmp(sql, "INSERT INTO", 11) == 0) {
        const char* p = sql + 11;
        while (*p == ' ') p++;
        char tname[MAX_STR_LEN];
        int tidx = 0;
        while (*p && *p != ' ' && tidx < MAX_STR_LEN - 1) {
            tname[tidx++] = *p++;
        }
        tname[tidx] = '\0';

        table_t* tbl = NULL;
        for (int i = 0; i < MAX_TABLES; i++) {
            if (current_db.tables[i].is_used && strcmp(current_db.tables[i].name, tname) == 0) {
                tbl = &current_db.tables[i];
                break;
            }
        }
        if (!tbl) {
            printf("Error: no such table: %s\n", tname);
            return -1;
        }
        if (tbl->num_rows >= MAX_ROWS) {
            printf("Error: table %s is full\n", tname);
            return -1;
        }

        const char* paren = strstr(p, "VALUES");
        if (!paren) paren = strstr(p, "values");
        if (paren) paren = strchr(paren, '(');

        if (paren) {
            paren++;
            row_t* row = &tbl->rows[tbl->num_rows];
            int col_idx = 0;
            while (*paren && *paren != ')' && col_idx < tbl->num_cols) {
                while (*paren == ' ' || *paren == ',') paren++;
                char vbuf[MAX_STR_LEN];
                int vidx = 0;
                char quote = 0;
                if (*paren == '\'' || *paren == '"') quote = *paren++;
                while (*paren && ((quote && *paren != quote) || (!quote && *paren != ',' && *paren != ')' && *paren != ' ')) && vidx < MAX_STR_LEN - 1) {
                    vbuf[vidx++] = *paren++;
                }
                vbuf[vidx] = '\0';
                if (quote && *paren == quote) paren++;
                strncpy(row->values[col_idx++], vbuf, MAX_STR_LEN - 1);
            }
            tbl->num_rows++;
            db_save();
            return 0;
        }
    }

    // SELECT * FROM name [WHERE col = val]
    if (strncasecmp(sql, "SELECT", 6) == 0) {
        const char* from = strstr(sql, "FROM");
        if (!from) from = strstr(sql, "from");
        if (!from) {
            printf("Error: SELECT syntax error (missing FROM)\n");
            return -1;
        }
        from += 4;
        while (*from == ' ') from++;
        char tname[MAX_STR_LEN];
        int tidx = 0;
        while (*from && *from != ' ' && *from != ';' && tidx < MAX_STR_LEN - 1) {
            tname[tidx++] = *from++;
        }
        tname[tidx] = '\0';

        table_t* tbl = NULL;
        for (int i = 0; i < MAX_TABLES; i++) {
            if (current_db.tables[i].is_used && strcmp(current_db.tables[i].name, tname) == 0) {
                tbl = &current_db.tables[i];
                break;
            }
        }
        if (!tbl) {
            printf("Error: no such table: %s\n", tname);
            return -1;
        }

        // Print header
        for (int c = 0; c < tbl->num_cols; c++) {
            printf("%-15s", tbl->cols[c].name);
        }
        printf("\n");
        for (int c = 0; c < tbl->num_cols; c++) {
            printf("---------------");
        }
        printf("\n");

        // Print rows
        for (int r = 0; r < tbl->num_rows; r++) {
            for (int c = 0; c < tbl->num_cols; c++) {
                printf("%-15s", tbl->rows[r].values[c]);
            }
            printf("\n");
        }
        return 0;
    }

    // DROP TABLE name
    if (strncasecmp(sql, "DROP TABLE", 10) == 0) {
        const char* p = sql + 10;
        while (*p == ' ') p++;
        char tname[MAX_STR_LEN];
        int tidx = 0;
        while (*p && *p != ' ' && *p != ';' && tidx < MAX_STR_LEN - 1) {
            tname[tidx++] = *p++;
        }
        tname[tidx] = '\0';

        for (int i = 0; i < MAX_TABLES; i++) {
            if (current_db.tables[i].is_used && strcmp(current_db.tables[i].name, tname) == 0) {
                current_db.tables[i].is_used = 0;
                db_save();
                return 0;
            }
        }
        printf("Error: no such table: %s\n", tname);
        return -1;
    }

    return 0;
}

int sqlite_main(int argc, char** argv) {
    const char* dbname = ":memory:";
    if (argc > 1 && argv[1]) {
        dbname = argv[1];
    }
    db_load(dbname);

    printf("SQLite version 3.44.2 (LemonOS Baremetal Edition)\n");
    printf("Enter \".help\" for usage hints.\n");
    printf("Connected to %s\n\n", dbname);

    char buf[256];
    while (1) {
        printf("sqlite> ");
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
            } else if ((unsigned char)c >= 32 && (unsigned char)c <= 126 && len < 254) {
                buf[len++] = c;
                buf[len] = '\0';
                vga_putchar(c);
            }
        }

        if (strcmp(buf, ".quit") == 0 || strcmp(buf, ".exit") == 0) {
            break;
        }

        if (buf[0]) {
            sqlite_exec(dbname, buf);
        }
    }

    return 0;
}
