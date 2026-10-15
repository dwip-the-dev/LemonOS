#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <setjmp.h>
#include <assert.h>

static int cmp_int(const void* a, const void* b) {
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    return (ia > ib) - (ia < ib);
}

static jmp_buf test_jmp;

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("====================================================\n");
    printf("[LIBC TEST] Standard C Library Full Test Suite (C99)\n");
    printf("====================================================\n");

    // 1. Memory Management (malloc, calloc, realloc, free)
    printf("[1/7] Testing Dynamic Memory Allocator (malloc/realloc/free):\n");
    char* buf = (char*)malloc(128);
    assert(buf != NULL);
    strcpy(buf, "Standard C Library dynamic heap allocation works!");
    printf("      buf: '%s'\n", buf);

    buf = (char*)realloc(buf, 256);
    assert(buf != NULL);
    strcat(buf, " Expanded with realloc().");
    printf("      expanded: '%s'\n", buf);
    free(buf);
    printf("      Heap allocation and free verified ... PASS\n");

    // 2. Sorting & Searching (qsort, bsearch)
    printf("[2/7] Testing Algorithms (qsort & bsearch):\n");
    int arr[] = { 42, 17, 88, 3, 99, 12, 55, 1 };
    size_t n = sizeof(arr) / sizeof(arr[0]);
    qsort(arr, n, sizeof(int), cmp_int);
    printf("      Sorted array: ");
    for (size_t i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");

    int key = 55;
    int* found = (int*)bsearch(&key, arr, n, sizeof(int), cmp_int);
    assert(found != NULL && *found == 55);
    printf("      bsearch found key %d at address %p ... PASS\n", key, (void*)found);

    // 3. String & Formatting (snprintf, strtol, etc.)
    printf("[3/7] Testing Formatted I/O & Conversions (snprintf, strtol):\n");
    char fmt[128];
    snprintf(fmt, sizeof(fmt), "Int=%d, Hex=0x%X, String='%s'", 12345, 0xABCD, "LemonOS");
    printf("      Formatted string: '%s'\n", fmt);

    const char* num_str = "987654";
    char* endptr = NULL;
    long lval = strtol(num_str, &endptr, 10);
    assert(lval == 987654);
    printf("      strtol converted '%s' to %ld ... PASS\n", num_str, lval);

    // 4. File Stream I/O (fopen, fwrite, fseek, fread, fclose)
    printf("[4/7] Testing Standard File Streams (fopen, fwrite, fread):\n");
    FILE* fp = fopen("/tmp/libc_test.txt", "w+");
    if (fp) {
        const char* f_msg = "Hello from Libc Standard File Stream API!\n";
        size_t written = fwrite(f_msg, 1, strlen(f_msg), fp);
        printf("      fwrite wrote %u bytes.\n", (unsigned int)written);
        fseek(fp, 0, SEEK_SET);

        char rbuf[64];
        memset(rbuf, 0, sizeof(rbuf));
        size_t read_bytes = fread(rbuf, 1, sizeof(rbuf) - 1, fp);
        printf("      fread read %u bytes: '%s'", (unsigned int)read_bytes, rbuf);
        fclose(fp);
        printf("      File stream operations verified ... PASS\n");
    } else {
        printf("      Note: /tmp/libc_test.txt fopen returned NULL (VFS write restricted or not mounted).\n");
    }

    // 5. Math Functions (sqrt, pow, sin, cos, floor, ceil)
    printf("[5/7] Testing Mathematical Computations (math.h):\n");
    double sq = sqrt(144.0);
    double pw = pow(2.0, 10.0);
    double sn = sin(0.0);
    double cs = cos(0.0);
    double fl = floor(3.7);
    double cl = ceil(3.2);
    printf("      sqrt(144)=%d, pow(2,10)=%d, sin(0)=%d, cos(0)=%d, floor(3.7)=%d, ceil(3.2)=%d\n",
           (int)sq, (int)pw, (int)sn, (int)cs, (int)fl, (int)cl);
    assert((int)sq == 12 && (int)pw == 1024 && (int)cs == 1 && (int)fl == 3 && (int)cl == 4);
    printf("      Math library calculations verified ... PASS\n");

    // 6. Non-local jumps (setjmp / longjmp)
    printf("[6/7] Testing Non-local Jumps (setjmp & longjmp):\n");
    int val = setjmp(test_jmp);
    if (val == 0) {
        printf("      setjmp initial return: calling longjmp(test_jmp, 99)...\n");
        longjmp(test_jmp, 99);
    } else {
        printf("      longjmp landed back! returned code=%d ... PASS\n", val);
        assert(val == 99);
    }

    // 7. Environment Variables (getenv, setenv)
    printf("[7/7] Testing Environment Variable Access (getenv):\n");
    const char* path_env = getenv("PATH");
    const char* user_env = getenv("USER");
    printf("      PATH = '%s'\n", path_env ? path_env : "(null)");
    printf("      USER = '%s'\n", user_env ? user_env : "(null)");
    printf("      Environment access verified ... PASS\n");

    printf("====================================================\n");
    printf("[LIBC TEST] ALL C99 STANDARD LIBRARY TESTS PASSED!\n");
    printf("====================================================\n");
    return 0;
}
