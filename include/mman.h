#ifndef MMAN_H
#define MMAN_H

#include <stdint.h>
#include <stddef.h>

#define PROT_NONE       0x0
#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4

#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_ANON        MAP_ANONYMOUS

#define MS_ASYNC        1
#define MS_SYNC         2
#define MS_INVALIDATE   4

#define MAP_FAILED      ((void*)-1)
#define MMAP_PAGE_SIZE  4096

typedef int32_t off_t;

void  mman_init(void);
void* mman_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
int   mman_munmap(void* addr, size_t length);
int   mman_msync(void* addr, size_t length, int flags);

// Verification test suite
void  mman_run_tests(void);

#endif
