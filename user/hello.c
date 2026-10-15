#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    printf("\n===============================================\n");
    printf("  Hello from LemonOS Userspace (Ring 3)!       \n");
    printf("===============================================\n");
    printf("Standard C Library (Libc) & POSIX Runtime Active!\n");
    printf("Process PID: %d, PPID: %d\n", getpid(), getppid());
    printf("Command line arguments (argc=%d):\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("  argv[%d] = %s\n", i, argv[i] ? argv[i] : "(null)");
    }
    return 0;
}
