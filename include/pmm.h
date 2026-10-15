#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PMM_PAGE_SIZE       4096
#define PMM_PAGE_ALIGN_MASK (~(PMM_PAGE_SIZE - 1))

// Max supported physical memory: 1024 MB (262,144 physical frames = 1 GB)
#define PMM_MAX_RAM_MB      1024
#define PMM_TOTAL_FRAMES    ((PMM_MAX_RAM_MB * 1024 * 1024) / PMM_PAGE_SIZE)
#define PMM_BITMAP_DWORDS   (PMM_TOTAL_FRAMES / 32)

// Initialize Physical Memory Manager
void     pmm_init(uint32_t mem_size_kb);

// Allocate a single 4KB physical frame (returns physical address, 0 on failure)
uintptr_t pmm_alloc_frame(void);

// Free a previously allocated 4KB physical frame
void     pmm_free_frame(uintptr_t phys_addr);

// Allocate multiple contiguous physical frames (essential for DMA, network ring buffers)
uintptr_t pmm_alloc_frames(size_t count);

// Free multiple contiguous physical frames
void     pmm_free_frames(uintptr_t phys_addr, size_t count);

// Mark a specific physical memory range as reserved (e.g. BIOS, Kernel code, VRAM)
void     pmm_reserve_region(uintptr_t base_addr, size_t length);

// Telemetry & Statistics
size_t   pmm_get_total_frames(void);
size_t   pmm_get_free_frames(void);
size_t   pmm_get_used_frames(void);
size_t   pmm_get_free_memory_kb(void);

// Self-Test Suite
void     pmm_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif // PMM_H
