// Shared C Library for LemonOS Dynamic Linker Demo
#include <stdint.h>

// Forward declaration of syscalls or wrapper
static inline int sys_write(int fd, const void* buf, int count) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(2), "b"(fd), "c"(buf), "d"(count) : "memory");
    return ret;
}

static inline void sys_exit(int code) {
    asm volatile("int $0x80" : : "a"(0), "b"(code) : "memory");
    while(1) {}
}

int puts(const char* s) {
    if (!s) return 0;
    int len = 0;
    while (s[len]) len++;
    sys_write(1, s, len);
    sys_write(1, "\n", 1);
    return len;
}

void exit(int status) {
    sys_exit(status);
}

int getpid(void) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(5) : "memory");
    return ret;
}
