#include "stdlib.h"
#include "string.h"
#include "kmem.h"

void* malloc(size_t size) {
    return kmalloc(size);
}

void free(void* ptr) {
    kfree(ptr);
}

void* calloc(size_t num, size_t size) {
    return kcalloc(num, size);
}

void* realloc(void* ptr, size_t size) {
    return krealloc(ptr, size);
}

void abort(void) {
    // Hang the system
    while (1) {
        asm volatile ("hlt");
    }
}

int atoi(const char* str) {
    if (!str) return 0;
    while (*str == ' ' || *str == '\t') str++;
    int result = 0;
    int sign = 1;
    
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

long atol(const char* str) {
    return (long)atoi(str);
}

double atof(const char* str) {
    if (!str) return 0.0;
    while (*str == ' ' || *str == '\t') str++;
    double sign = 1.0;
    if (*str == '-') {
        sign = -1.0;
        str++;
    } else if (*str == '+') {
        str++;
    }
    double val = 0.0;
    while (*str >= '0' && *str <= '9') {
        val = val * 10.0 + (*str - '0');
        str++;
    }
    if (*str == '.') {
        str++;
        double factor = 0.1;
        while (*str >= '0' && *str <= '9') {
            val += (*str - '0') * factor;
            factor *= 0.1;
            str++;
        }
    }
    return sign * val;
}

int system(const char* command) {
    (void)command;
    return -1;
}

int abs(int x) {
    return (x < 0) ? -x : x;
}

long labs(long x) {
    return (x < 0) ? -x : x;
}

static unsigned long next_rand = 1;

int rand(void) {
    next_rand = next_rand * 1103515245 + 12345;
    return (unsigned int)(next_rand / 65536) % 32768;
}

void srand(unsigned int seed) {
    next_rand = seed;
}

char* getenv(const char* name) {
    (void)name;
    return NULL;
}

static void swap_bytes(char *a, char *b, size_t size) {
    while (size--) {
        char tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    if (nmemb <= 1 || size == 0) return;
    char *arr = (char *)base;
    for (size_t i = 0; i < nmemb - 1; i++) {
        size_t min_idx = i;
        for (size_t j = i + 1; j < nmemb; j++) {
            if (compar(arr + j * size, arr + min_idx * size) < 0) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            swap_bytes(arr + i * size, arr + min_idx * size, size);
        }
    }
}

void exit(int status) {
    (void)status;
    abort();
}

// 64-bit Unsigned Integer Division for 32-bit x86 bare metal
uint64_t __udivdi3(uint64_t n, uint64_t d) {
    if (d == 0) return 0;
    uint64_t q = 0;
    uint64_t r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) {
            r -= d;
            q |= ((uint64_t)1 << i);
        }
    }
    return q;
}

uint64_t __umoddi3(uint64_t n, uint64_t d) {
    if (d == 0) return 0;
    uint64_t r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) {
            r -= d;
        }
    }
    return r;
}

int64_t __divdi3(int64_t a, int64_t b) {
    int neg = 0;
    if (a < 0) {
        a = -a;
        neg = !neg;
    }
    if (b < 0) {
        b = -b;
        neg = !neg;
    }
    uint64_t res = __udivdi3((uint64_t)a, (uint64_t)b);
    return neg ? -(int64_t)res : (int64_t)res;
}

int64_t __moddi3(int64_t a, int64_t b) {
    int neg = 0;
    if (a < 0) {
        a = -a;
        neg = 1;
    }
    if (b < 0) {
        b = -b;
    }
    uint64_t res = __umoddi3((uint64_t)a, (uint64_t)b);
    return neg ? -(int64_t)res : (int64_t)res;
}

int errno = 0;

int* __errno_location(void) {
    return &errno;
}

int mkdir(const char *pathname, unsigned int mode) {
    (void)pathname;
    (void)mode;
    return 0;
}

// Ctype lookup tables for glibc ABI compatibility
static unsigned short ctype_b_table[384];
static int ctype_toupper_table[384];
static int ctype_tolower_table[384];
static int ctype_inited = 0;

static void init_ctype_tables(void) {
    if (ctype_inited) return;
    for (int i = 0; i < 256; i++) {
        unsigned short b = 0;
        if (i >= '0' && i <= '9') b |= 0x0800; // _ISdigit
        if (i >= 'a' && i <= 'z') b |= 0x0200; // _ISlower
        if (i >= 'A' && i <= 'Z') b |= 0x0100; // _ISupper
        if (b & 0x0300) b |= 0x0400;           // _ISalpha
        if (b & 0x0C00) b |= 0x0008;           // _ISalnum
        if (i == ' ' || (i >= 9 && i <= 13)) b |= 0x2000; // _ISspace
        if (i >= 32 && i <= 126) b |= 0x4000;  // _ISprint
        ctype_b_table[128 + i] = b;
        ctype_toupper_table[128 + i] = (i >= 'a' && i <= 'z') ? (i - 32) : i;
        ctype_tolower_table[128 + i] = (i >= 'A' && i <= 'Z') ? (i + 32) : i;
    }
    ctype_inited = 1;
}

const unsigned short *__ctype_b_loc(void) {
    init_ctype_tables();
    return &ctype_b_table[128];
}

const int *__ctype_toupper_loc(void) {
    init_ctype_tables();
    return &ctype_toupper_table[128];
}

const int *__ctype_tolower_loc(void) {
    init_ctype_tables();
    return &ctype_tolower_table[128];
}
