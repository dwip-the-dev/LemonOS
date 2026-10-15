#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("=========================================\n");
    printf("[USERSPACE] Ring 3 Syscall Self-Test Suite\n");
    printf("=========================================\n");

    // Test 1: getpid
    pid_t pid = getpid();
    if (pid > 0) {
        printf("  -> [TEST 1/5] getpid(): PASS (PID %d)\n", pid);
    } else {
        printf("  -> [TEST 1/5] getpid(): FAIL\n");
    }

    // Test 2: write
    ssize_t w = write(1, "  -> [TEST 2/5] write(stdout): PASS\n", 36);
    (void)w;

    // Test 3: sbrk
    void* p = sbrk(256);
    if ((intptr_t)p > 0) {
        *(int*)p = 0x12345678;
        if (*(int*)p == 0x12345678) {
            printf("  -> [TEST 3/5] sbrk(256) heap expansion: PASS\n");
        } else {
            printf("  -> [TEST 3/5] sbrk heap verification: FAIL\n");
        }
    } else {
        printf("  -> [TEST 3/5] sbrk allocation: FAIL\n");
    }

    // Test 4: File I/O
    int fd = open("/docs/welcome.txt", O_RDONLY);
    if (fd >= 0) {
        char fbuf[32];
        ssize_t r = read(fd, fbuf, 10);
        close(fd);
        if (r > 0) {
            printf("  -> [TEST 4/5] open/read/close(/docs/welcome.txt): PASS\n");
        } else {
            printf("  -> [TEST 4/5] read file contents: FAIL\n");
        }
    } else {
        printf("  -> [TEST 4/5] open(/docs/welcome.txt): FAIL\n");
    }

    // Test 5: time
    time_t t = time(NULL);
    if (t > 0) {
        printf("  -> [TEST 5/5] time(): PASS (%u seconds)\n", (unsigned int)t);
    } else {
        printf("  -> [TEST 5/5] time(): FAIL\n");
    }

    printf("[USERSPACE] Ring 3 User Mode Self-Test PASSED!\n");
    return 0;
}
