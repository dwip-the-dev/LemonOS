#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv) {
    printf("[DYNPOSIX] Dynamically linked POSIX executable running!\n");
    printf("[DYNPOSIX] argc=%d, argv[0]='%s'\n", argc, argv[0] ? argv[0] : "(null)");
    pid_t p = getpid();
    printf("[DYNPOSIX] Calling dynamic getpid(): %d\n", p);
    char* mem = (char*)malloc(64);
    if (mem) {
        printf("[DYNPOSIX] Dynamic heap malloc allocated at %p\n", (void*)mem);
        free(mem);
    }
    printf("[DYNPOSIX] Dynamic linking test SUCCESS!\n");
    return 0;
}
