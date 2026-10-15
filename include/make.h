#ifndef MAKE_H
#define MAKE_H

#include <stdint.h>
#include <stddef.h>

#define MAKE_MAX_TARGETS   16
#define MAKE_MAX_DEPS      8
#define MAKE_MAX_COMMANDS  8
#define MAKE_MAX_VARS      16

typedef struct {
    char target[32];
    char deps[MAKE_MAX_DEPS][32];
    int  dep_count;
    char commands[MAKE_MAX_COMMANDS][96];
    int  command_count;
} make_rule_t;

typedef struct {
    char key[32];
    char val[64];
} make_var_t;

void make_init(void);
int  make_run(const char* makefile_path, const char* target_name, int dry_run);
int  make_cli(int argc, char** argv);

// Verification tests
void make_run_tests(void);

#endif
