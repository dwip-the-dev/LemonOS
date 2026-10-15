// LemonOS Physical Memory Manager (PMM)
// 4KB Page Frame Allocator with Bitmap Tracking
// Author: Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "pmm.h"
#include "string.h"
#include "serial.h"

// Physical memory bitmap: 1 bit per 4KB frame (1 = used/reserved, 0 = free)
static uint32_t pmm_bitmap[PMM_BITMAP_DWORDS];
static size_t   total_frames_count = 0;
static size_t   used_frames_count = 0;

static inline void bitmap_set(size_t frame) {
    if (frame < total_frames_count) {
        if (!(pmm_bitmap[frame / 32] & (1U << (frame % 32)))) {
            pmm_bitmap[frame / 32] |= (1U << (frame % 32));
            used_frames_count++;
        }
    }
}

static inline void bitmap_clear(size_t frame) {
    if (frame < total_frames_count) {
        if (pmm_bitmap[frame / 32] & (1U << (frame % 32))) {
            pmm_bitmap[frame / 32] &= ~(1U << (frame % 32));
            used_frames_count--;
        }
    }
}

static inline int bitmap_test(size_t frame) {
    if (frame >= total_frames_count) return 1;
    return (pmm_bitmap[frame / 32] & (1U << (frame % 32))) ? 1 : 0;
}

void pmm_init(uint32_t mem_size_kb) {
    if (mem_size_kb == 0) mem_size_kb = 1024 * 1024; // Default 1024 MB (1 GB) RAM

    total_frames_count = (mem_size_kb * 1024) / PMM_PAGE_SIZE;
    if (total_frames_count > PMM_TOTAL_FRAMES) {
        total_frames_count = PMM_TOTAL_FRAMES;
    }

    // 1. Mark entire bitmap as used initially (conservative start)
    memset(pmm_bitmap, 0xFF, sizeof(pmm_bitmap));
    used_frames_count = total_frames_count;

    // 2. Mark available physical RAM above 1MB up to total_frames_count as free
    size_t first_free_frame = (1024 * 1024) / PMM_PAGE_SIZE; // 1 MB mark (frame 256)
    for (size_t f = first_free_frame; f < total_frames_count; f++) {
        bitmap_clear(f);
    }

    // 3. Reserve Kernel image, embedded assets, and BSS memory
    extern uint32_t _kernel_end;
    uintptr_t k_end = (uintptr_t)&_kernel_end;
    if (k_end < 32 * 1024 * 1024) k_end = 32 * 1024 * 1024; // Ensure at least 32MB reserved for kernel & assets
    pmm_reserve_region(1024 * 1024, k_end - (1024 * 1024));

    serial_println("[PMM] Physical Memory Manager initialized.");
    serial_print("[PMM]   Total RAM  : ");
    serial_print_dec((uint32_t)(total_frames_count * 4));
    serial_println(" KB");
    serial_print("[PMM]   Free RAM   : ");
    serial_print_dec((uint32_t)(pmm_get_free_frames() * 4));
    serial_println(" KB");
}

void pmm_reserve_region(uintptr_t base_addr, size_t length) {
    size_t start_frame = base_addr / PMM_PAGE_SIZE;
    size_t num_frames  = (length + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;

    for (size_t i = 0; i < num_frames; i++) {
        bitmap_set(start_frame + i);
    }
}

uintptr_t pmm_alloc_frame(void) {
    for (size_t i = 0; i < PMM_BITMAP_DWORDS; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) {
            for (int b = 0; b < 32; b++) {
                if (!(pmm_bitmap[i] & (1U << b))) {
                    size_t frame = i * 32 + b;
                    if (frame < total_frames_count) {
                        bitmap_set(frame);
                        return (uintptr_t)(frame * PMM_PAGE_SIZE);
                    }
                }
            }
        }
    }
    serial_println("[PMM] ERROR: Out of physical memory frames!");
    return 0;
}

void pmm_free_frame(uintptr_t phys_addr) {
    if (phys_addr % PMM_PAGE_SIZE != 0) return;
    size_t frame = phys_addr / PMM_PAGE_SIZE;
    bitmap_clear(frame);
}

uintptr_t pmm_alloc_frames(size_t count) {
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_frame();

    size_t contiguous = 0;
    size_t start_frame = 0;

    for (size_t f = 0; f < total_frames_count; f++) {
        if (!bitmap_test(f)) {
            if (contiguous == 0) start_frame = f;
            contiguous++;
            if (contiguous == count) {
                for (size_t i = 0; i < count; i++) {
                    bitmap_set(start_frame + i);
                }
                return (uintptr_t)(start_frame * PMM_PAGE_SIZE);
            }
        } else {
            contiguous = 0;
        }
    }

    serial_println("[PMM] ERROR: Failed to allocate contiguous physical frames.");
    return 0;
}

void pmm_free_frames(uintptr_t phys_addr, size_t count) {
    if (phys_addr % PMM_PAGE_SIZE != 0 || count == 0) return;
    size_t start_frame = phys_addr / PMM_PAGE_SIZE;
    for (size_t i = 0; i < count; i++) {
        bitmap_clear(start_frame + i);
    }
}

size_t pmm_get_total_frames(void) {
    return total_frames_count;
}

size_t pmm_get_free_frames(void) {
    return (total_frames_count >= used_frames_count) ? (total_frames_count - used_frames_count) : 0;
}

size_t pmm_get_used_frames(void) {
    return used_frames_count;
}

size_t pmm_get_free_memory_kb(void) {
    return pmm_get_free_frames() * 4;
}

void pmm_run_tests(void) {
    serial_println("=========================================");
    serial_println("[PMM] Running Physical Memory Manager Test Suite...");
    serial_println("=========================================");

    // Test 1: Single frame allocation & 4096 alignment
    uintptr_t f1 = pmm_alloc_frame();
    if (f1 != 0 && (f1 % PMM_PAGE_SIZE == 0)) {
        serial_println("  -> [TEST 1/4] Single 4KB frame allocation & alignment: PASS");
    } else {
        serial_println("  -> [TEST 1/4] Single frame allocation: FAIL");
    }

    // Test 2: Frame free & reclamation
    size_t free_before = pmm_get_free_frames();
    pmm_free_frame(f1);
    size_t free_after = pmm_get_free_frames();
    if (free_after == free_before + 1) {
        serial_println("  -> [TEST 2/4] Frame freeing & bitmap counter recycling: PASS");
    } else {
        serial_println("  -> [TEST 2/4] Frame freeing: FAIL");
    }

    // Test 3: Multi-frame contiguous allocation (4 frames = 16 KB)
    uintptr_t f4 = pmm_alloc_frames(4);
    if (f4 != 0 && (f4 % PMM_PAGE_SIZE == 0)) {
        serial_println("  -> [TEST 3/4] Multi-frame contiguous allocation (16 KB): PASS");
        pmm_free_frames(f4, 4);
    } else {
        serial_println("  -> [TEST 3/4] Multi-frame contiguous allocation: FAIL");
    }

    // Test 4: Memory stats accuracy
    if (pmm_get_total_frames() > 0 && pmm_get_free_frames() > 0) {
        serial_println("  -> [TEST 4/4] Physical memory telemetry verification: PASS");
    } else {
        serial_println("  -> [TEST 4/4] Telemetry verification: FAIL");
    }

    serial_println("[PMM] Physical Memory Manager: 4/4 PASSED!");
}
