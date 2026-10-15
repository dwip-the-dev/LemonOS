#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("--- Userspace System Information Utility ---\n");
    printf("Process PID:       %d\n", getpid());
    printf("Parent PID:        %d\n", getppid());
    printf("Privilege Level:   Ring 3 (CPL=3 User Mode)\n");
    printf("System Call API:   int 0x80 POSIX Dispatcher\n");
    printf("Standard Libc:     LemonOS Standalone Libc (C99/POSIX.1-2001)\n");

    printf("Testing sbrk(512): ");
    char* mem = (char*)sbrk(512);
    if ((intptr_t)mem > 0) {
        strcpy(mem, "LEMON");
        if (strcmp(mem, "LEMON") == 0) {
            printf("PASS (Heap allocated and verified in Ring 3!)\n");
        } else {
            printf("FAIL (Data corrupt)\n");
        }
    } else {
        printf("FAIL (sbrk returned error)\n");
    }

    return 0;
}
