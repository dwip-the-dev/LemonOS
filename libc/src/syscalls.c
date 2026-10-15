#include <errno.h>
#include <stdint.h>
#include <stddef.h>

int errno = 0;

int __syscall0(int num) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num) : "memory");
    return ret;
}

int __syscall1(int num, uint32_t a1) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1) : "memory");
    return ret;
}

int __syscall2(int num, uint32_t a1, uint32_t a2) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2) : "memory");
    return ret;
}

int __syscall3(int num, uint32_t a1, uint32_t a2, uint32_t a3) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2), "d"(a3) : "memory");
    return ret;
}

int __syscall4(int num, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2), "d"(a3), "S"(a4) : "memory");
    return ret;
}

int __syscall5(int num, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2), "d"(a3), "S"(a4), "D"(a5) : "memory");
    return ret;
}

int __syscall_ret(int ret) {
    if (ret < 0 && ret >= -4095) {
        errno = -ret;
        return -1;
    }
    return ret;
}
