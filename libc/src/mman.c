#include <sys/mman.h>
#include <errno.h>
#include "syscall_nums.h"

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    (void)offset;
    int ret = __syscall5(SYS_MMAP, (unsigned int)addr, (unsigned int)length, (unsigned int)prot, (unsigned int)flags, (unsigned int)fd);
    if (ret < 0) {
        errno = -ret;
        return MAP_FAILED;
    }
    return (void*)ret;
}

int munmap(void* addr, size_t length) {
    return __syscall_ret(__syscall2(SYS_MUNMAP, (unsigned int)addr, (unsigned int)length));
}

int mprotect(void* addr, size_t len, int prot) {
    (void)addr; (void)len; (void)prot;
    return 0;
}
