#ifndef LANGRT_H
#define LANGRT_H

#include <stdint.h>
#include <stddef.h>

// Language runtime identifiers
typedef enum {
    LANG_C = 0,
    LANG_CPP,
    LANG_ASM,      // x86 NASM Assembly
    LANG_PYTHON,
    LANG_JAVASCRIPT,
    LANG_TYPESCRIPT,
    LANG_JAVA,
    LANG_HTML,
    LANG_CSS,
    LANG_WASM,
    LANG_UNKNOWN
} language_t;

// Detect language from file extension
language_t langrt_detect(const char* filename);

// Get human-readable language name
const char* langrt_name(language_t lang);

// Initialize multi-language runtime
void langrt_init(void);

// Print callback for output routing (GUI terminal, VGA console, serial)
typedef void (*langrt_print_fn)(const char* line, uint32_t color);

// Core execution engine
int langrt_run_file(const char* filepath, langrt_print_fn pr);
int langrt_run_string(const char* source, language_t lang, langrt_print_fn pr);

// Language-specific interpreters and compilers
int langrt_py_interpret(const char* source, langrt_print_fn pr);
int langrt_js_interpret(const char* source, langrt_print_fn pr);
int langrt_cpp_compile(const char* source, const char* out_path, langrt_print_fn pr);
int langrt_asm_assemble(const char* source, const char* out_path, langrt_print_fn pr);
int langrt_java_compile_and_run(const char* source, const char* classname, langrt_print_fn pr);
int langrt_wasm_run(const char* source, langrt_print_fn pr);
int langrt_html_render(const char* source, langrt_print_fn pr);

// Universal command dispatcher for all language binaries
int langrt_exec_cmd(const char* cmd, const char* arg, langrt_print_fn pr);

// CLI front-end commands
int python_cli(int argc, char** argv);
int node_cli(int argc, char** argv);
int tsc_cli(int argc, char** argv);
int javac_cli(int argc, char** argv);
int java_cli(int argc, char** argv);
int nasm_cli(int argc, char** argv);
int gpp_cli(int argc, char** argv);  // g++ / clang++
int wasm_cli(int argc, char** argv);

// Self-tests
void langrt_run_tests(void);

#endif
