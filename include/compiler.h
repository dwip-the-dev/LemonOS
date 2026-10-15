#ifndef COMPILER_H
#define COMPILER_H

#include <stdint.h>
#include <stddef.h>

#define COMPILER_MAX_SRC_SIZE  65536
#define COMPILER_MAX_BIN_SIZE  32768
#define COMPILER_MAX_TOKENS    2048

typedef enum {
    COMPILER_FRONTEND_GCC = 0,
    COMPILER_FRONTEND_CLANG = 1
} compiler_frontend_t;

typedef struct {
    char source_path[64];
    char output_path[64];
    int  opt_level;
    int  warn_all;
    int  verbose;
    int  assemble_only;
    compiler_frontend_t frontend;
} compiler_opts_t;

void compiler_init(void);
int  compiler_compile_file(const char* src_path, const char* out_path, compiler_frontend_t frontend);
int  compiler_compile_string(const char* c_source, const char* out_path, compiler_frontend_t frontend);
int  compiler_exec_cli(int argc, char** argv, compiler_frontend_t frontend);

// Self-test verification suite
void compiler_run_tests(void);

#endif
