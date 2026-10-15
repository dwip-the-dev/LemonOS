// LemonOS Kernel Dynamic Memory & Slab Allocator (KMEM)
// Boundary-Tag Coalescing Arena + Fast O(1) Slab Bins
// Author: Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>

#ifndef KMEM_H
#define KMEM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Kernel Memory Telemetry
typedef struct {
    size_t total_heap_bytes;
    size_t used_heap_bytes;
    size_t free_heap_bytes;
    size_t peak_used_bytes;
    size_t alloc_count;
    size_t free_count;
    size_t small_bin_allocs;
    size_t large_arena_allocs;
    size_t slab_pages_count;
    size_t arena_pages_count;
} kmem_stats_t;

// Initialize the Dynamic Kernel Heap & Slab Manager
void   kmem_init(void);

// Allocate kernel memory of given size
void*  kmalloc(size_t size);

// Free allocated kernel memory
void   kfree(void* ptr);

// Allocate zero-initialized memory for count objects
void*  kcalloc(size_t num, size_t size);

// Reallocate memory block to new size
void*  krealloc(void* ptr, size_t new_size);

// Retrieve live memory telemetry
void   kmem_get_stats(kmem_stats_t* stats);

// Verify heap integrity (returns 1 if valid, 0 if corrupted)
int    kmem_check_integrity(void);

// Self-Test Suite
void   kmem_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif // KMEM_H
