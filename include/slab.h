#ifndef SLAB_H
#define SLAB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the C++ Kernel Slab & Object Pool Allocator
void  slab_init(void);

// Fast O(1) Slab allocation for objects up to 4096 bytes
void* slab_alloc(size_t size);

// Free an object back to its originating Slab cache
void  slab_free(void* ptr);

// Formats detailed allocator statistics (caches, active objects, memory usage)
int   slab_dump_stats(char* out_buf, size_t max);

// Verification and self-test suite
void  slab_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif // SLAB_H
