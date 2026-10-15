#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void __assert_fail(const char* expr, const char* file, int line, const char* func) {
    fprintf(stderr, "Assertion failed: %s (%s: %s: %d)\n", expr, file, func ? func : "?", line);
    abort();
}

void __attribute__((noreturn)) __stack_chk_fail(void) {
    fprintf(stderr, "*** stack smashing detected ***: terminated\n");
    abort();
}

void __attribute__((visibility("hidden"), noreturn)) __stack_chk_fail_local(void) {
    __stack_chk_fail();
}

