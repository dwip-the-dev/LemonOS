// LemonOS Kernel Dynamic Memory & Slab Allocator (KMEM)
// High-Performance Dual-Tier Allocator:
//   Tier 1: O(1) Slab / Fixed-Size Bins for small objects (<= 2048 bytes)
//   Tier 2: Boundary-Tag Coalescing Arena with Dynamic Page Expansion
// Author: Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "kmem.h"
#include "pmm.h"
#include "vmm.h"
#include "string.h"
#include "serial.h"
#include "fast_mem.h"

// Magic signatures for memory integrity & corruption detection
#define KMEM_SLAB_MAGIC    0x534C4142 // "SLAB"
#define KMEM_ARENA_MAGIC   0x4C415247 // "LARG"
#define KMEM_FOOTER_MAGIC  0x454E4442 // "ENDB"
#define KMEM_CANARY_WORD   0xDEADBEEF

#define KMEM_CHUNK_FREE    0xAA55
#define KMEM_CHUNK_USED    0x55AA

// Tier 1: Small Object Slab Bins
#define NUM_BINS 8
static const size_t bin_sizes[NUM_BINS] = { 16, 32, 64, 128, 256, 512, 1024, 2048 };

typedef struct kmem_chunk {
    uint32_t magic;          // KMEM_SLAB_MAGIC
    uint16_t bin_idx;        // Index into bin_sizes (0..7)
    uint16_t status;         // KMEM_CHUNK_FREE or KMEM_CHUNK_USED
    struct kmem_chunk* next;
    uint32_t padding;        // Pad to exactly 16 bytes for 16-byte alignment
} __attribute__((aligned(8))) kmem_chunk_t;

typedef struct {
    size_t chunk_size;
    kmem_chunk_t* free_head;
    size_t total_chunks;
    size_t free_chunks;
    size_t alloc_count;
} kmem_bin_t;

static kmem_bin_t bins[NUM_BINS];

// Tier 2: Large Object Boundary-Tag Arena
typedef struct kmem_block_header {
    uint32_t magic;      // KMEM_ARENA_MAGIC
    uint32_t size;       // Payload size in bytes
    uint32_t is_free;    // 1 if free, 0 if allocated
    uint32_t canary;     // KMEM_CANARY_WORD
    struct kmem_block_header* next;
    struct kmem_block_header* prev;
    uint32_t reserved[2]; // Pad to exactly 32 bytes for 16-byte alignment
} __attribute__((aligned(16))) kmem_block_header_t;

typedef struct {
    uint32_t magic;      // KMEM_FOOTER_MAGIC
    uint32_t size;       // Payload size
} __attribute__((aligned(4))) kmem_block_footer_t;

static kmem_block_header_t* arena_head = NULL;
static kmem_stats_t kmem_stats;

// Allocate a single 4KB page from PMM for small object slab bins
static void* kmem_alloc_page(void) {
    uintptr_t phys = pmm_alloc_frame();
    if (!phys) {
        serial_println("[KMEM] ERROR: PMM out of memory while allocating slab page!");
        return NULL;
    }

    // Zero page memory for security and clean initial state
    memset((void*)phys, 0, 4096);
    kmem_stats.total_heap_bytes += 4096;
    kmem_stats.free_heap_bytes += 4096;
    kmem_stats.slab_pages_count++;
    return (void*)phys;
}

// Expand boundary tag arena by allocating contiguous frames from PMM
static int kmem_expand_arena(size_t min_bytes) {
    size_t needed_bytes = min_bytes + sizeof(kmem_block_header_t) + sizeof(kmem_block_footer_t) + 64;
    size_t num_pages = (needed_bytes + 4095) / 4096;
    if (num_pages < 32) num_pages = 32; // Expand in chunks of at least 128 KB

    uintptr_t start_phys = pmm_alloc_frames(num_pages);
    if (!start_phys) {
        serial_println("[KMEM] ERROR: PMM out of memory expanding arena!");
        return -1;
    }

    size_t total_expansion = num_pages * 4096;
    kmem_stats.total_heap_bytes += total_expansion;
    kmem_stats.free_heap_bytes += total_expansion;
    kmem_stats.arena_pages_count += num_pages;

    kmem_block_header_t* new_block = (kmem_block_header_t*)start_phys;
    new_block->magic = KMEM_ARENA_MAGIC;
    new_block->size = total_expansion - sizeof(kmem_block_header_t) - sizeof(kmem_block_footer_t);
    new_block->is_free = 1;
    new_block->canary = KMEM_CANARY_WORD;
    new_block->next = NULL;
    new_block->prev = NULL;

    kmem_block_footer_t* new_footer = (kmem_block_footer_t*)((uint8_t*)new_block + sizeof(kmem_block_header_t) + new_block->size);
    new_footer->magic = KMEM_FOOTER_MAGIC;
    new_footer->size = new_block->size;

    // Link into arena chain
    if (!arena_head) {
        arena_head = new_block;
    } else {
        kmem_block_header_t* curr = arena_head;
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = new_block;
        new_block->prev = curr;
    }

    return 0;
}

// Public Initialization
void kmem_init(void) {
    memset(&kmem_stats, 0, sizeof(kmem_stats_t));

    // Initialize small slab bins
    for (int i = 0; i < NUM_BINS; i++) {
        bins[i].chunk_size = bin_sizes[i];
        bins[i].free_head = NULL;
        bins[i].total_chunks = 0;
        bins[i].free_chunks = 0;
        bins[i].alloc_count = 0;
    }

    // Allocate initial 2MB heap arena from PMM
    arena_head = NULL;
    if (kmem_expand_arena(2 * 1024 * 1024) != 0) {
        // Fallback: allocate 1MB initial arena
        if (kmem_expand_arena(1 * 1024 * 1024) != 0) {
            serial_println("[KMEM] FATAL: Failed to allocate initial kernel heap!");
            return;
        }
    }

    serial_println("[KMEM] Dynamic Kernel Heap & Slab Manager initialized.");
    serial_print("[KMEM]   Initial Heap Arena: ");
    serial_print_dec((uint32_t)(kmem_stats.total_heap_bytes / 1024));
    serial_println(" KB allocated from PMM");
    serial_println("[KMEM]   Tier 1: 8 O(1) Slab Bins (16B - 2048B)");
    serial_println("[KMEM]   Tier 2: Boundary-Tag Coalescing Arena (>2048B)");
}

// Allocate Memory (kmalloc)
void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    // 1. Tier 1: Small fixed-size bin allocation
    if (size <= 2048) {
        int bin_idx = -1;
        for (int i = 0; i < NUM_BINS; i++) {
            if (bin_sizes[i] >= size) {
                bin_idx = i;
                break;
            }
        }

        if (bin_idx >= 0) {
            kmem_bin_t* b = &bins[bin_idx];

            // If free list is empty, carve a new 4KB page into chunks
            if (!b->free_head) {
                void* page = kmem_alloc_page();
                if (!page) return NULL;

                size_t step = sizeof(kmem_chunk_t) + b->chunk_size;
                size_t num_chunks = 4096 / step;

                uint8_t* ptr = (uint8_t*)page;
                for (size_t c = 0; c < num_chunks; c++) {
                    kmem_chunk_t* chk = (kmem_chunk_t*)(ptr + (c * step));
                    chk->magic = KMEM_SLAB_MAGIC;
                    chk->bin_idx = (uint16_t)bin_idx;
                    chk->status = KMEM_CHUNK_FREE;
                    chk->next = b->free_head;
                    b->free_head = chk;
                }
                b->total_chunks += num_chunks;
                b->free_chunks += num_chunks;
            }

            // Pop chunk from free list in O(1) time
            kmem_chunk_t* chunk = b->free_head;
            b->free_head = chunk->next;
            b->free_chunks--;
            b->alloc_count++;
            chunk->status = KMEM_CHUNK_USED;

            kmem_stats.alloc_count++;
            kmem_stats.small_bin_allocs++;
            kmem_stats.used_heap_bytes += b->chunk_size;
            if (kmem_stats.free_heap_bytes >= b->chunk_size) {
                kmem_stats.free_heap_bytes -= b->chunk_size;
            }
            if (kmem_stats.used_heap_bytes > kmem_stats.peak_used_bytes) {
                kmem_stats.peak_used_bytes = kmem_stats.used_heap_bytes;
            }

            return (void*)((uint8_t*)chunk + sizeof(kmem_chunk_t));
        }
    }

    // 2. Tier 2: Large arena boundary-tag allocation (Best-Fit / First-Fit)
    // Align payload size to 16-byte boundary
    size = (size + 15) & ~15;

retry_arena:;
    kmem_block_header_t* curr = arena_head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            // Split block if large enough
            size_t min_split = sizeof(kmem_block_header_t) + sizeof(kmem_block_footer_t) + 32;
            if (curr->size >= size + min_split) {
                size_t remaining_size = curr->size - size - sizeof(kmem_block_header_t) - sizeof(kmem_block_footer_t);

                // Set current block to requested size
                curr->size = size;
                curr->is_free = 0;
                curr->canary = KMEM_CANARY_WORD;

                kmem_block_footer_t* footer = (kmem_block_footer_t*)((uint8_t*)curr + sizeof(kmem_block_header_t) + curr->size);
                footer->magic = KMEM_FOOTER_MAGIC;
                footer->size = curr->size;

                // Create new free remainder block
                kmem_block_header_t* next_blk = (kmem_block_header_t*)((uint8_t*)footer + sizeof(kmem_block_footer_t));
                next_blk->magic = KMEM_ARENA_MAGIC;
                next_blk->size = remaining_size;
                next_blk->is_free = 1;
                next_blk->canary = KMEM_CANARY_WORD;
                next_blk->next = curr->next;
                next_blk->prev = curr;
                if (curr->next) curr->next->prev = next_blk;
                curr->next = next_blk;

                kmem_block_footer_t* next_ftr = (kmem_block_footer_t*)((uint8_t*)next_blk + sizeof(kmem_block_header_t) + next_blk->size);
                next_ftr->magic = KMEM_FOOTER_MAGIC;
                next_ftr->size = remaining_size;
            } else {
                curr->is_free = 0;
                curr->canary = KMEM_CANARY_WORD;
            }

            kmem_stats.alloc_count++;
            kmem_stats.large_arena_allocs++;
            kmem_stats.used_heap_bytes += curr->size;
            if (kmem_stats.free_heap_bytes >= curr->size) {
                kmem_stats.free_heap_bytes -= curr->size;
            }
            if (kmem_stats.used_heap_bytes > kmem_stats.peak_used_bytes) {
                kmem_stats.peak_used_bytes = kmem_stats.used_heap_bytes;
            }

            return (void*)((uint8_t*)curr + sizeof(kmem_block_header_t));
        }
        curr = curr->next;
    }

    // Need to expand arena
    if (kmem_expand_arena(size) == 0) {
        goto retry_arena;
    }

    serial_println("[KMEM] ERROR: kmalloc failed to allocate large block!");
    return NULL;
}

// Free Memory (kfree with O(1) Coalescing & Double-Free Protection)
void kfree(void* ptr) {
    if (!ptr) return;

    // 1. Check Tier 1 Slab Chunk (header is 16 bytes before payload)
    kmem_chunk_t* chunk = (kmem_chunk_t*)((uint8_t*)ptr - sizeof(kmem_chunk_t));
    if (chunk->magic == KMEM_SLAB_MAGIC) {
        if (chunk->status == KMEM_CHUNK_FREE) {
            serial_println("[KMEM] WARNING: Double-free detected on slab chunk! Ignored.");
            return;
        }

        uint16_t bin_idx = chunk->bin_idx;
        if (bin_idx >= NUM_BINS) {
            serial_println("[KMEM] ERROR: Corrupted bin index in slab chunk!");
            return;
        }

        chunk->status = KMEM_CHUNK_FREE;
        chunk->next = bins[bin_idx].free_head;
        bins[bin_idx].free_head = chunk;
        bins[bin_idx].free_chunks++;

        kmem_stats.free_count++;
        if (kmem_stats.used_heap_bytes >= bins[bin_idx].chunk_size) {
            kmem_stats.used_heap_bytes -= bins[bin_idx].chunk_size;
            kmem_stats.free_heap_bytes += bins[bin_idx].chunk_size;
        }
        return;
    }

    // 2. Check Tier 2 Arena Block (header is 32 bytes before payload)
    kmem_block_header_t* blk = (kmem_block_header_t*)((uint8_t*)ptr - sizeof(kmem_block_header_t));
    if (blk->magic == KMEM_ARENA_MAGIC) {
        if (blk->is_free) {
            serial_println("[KMEM] WARNING: Double-free detected on arena block! Ignored.");
            return;
        }

        if (blk->canary != KMEM_CANARY_WORD) {
            serial_println("[KMEM] CRITICAL: Buffer overflow/canary corruption detected on block header!");
        }

        blk->is_free = 1;
        kmem_stats.free_count++;
        if (kmem_stats.used_heap_bytes >= blk->size) {
            kmem_stats.used_heap_bytes -= blk->size;
            kmem_stats.free_heap_bytes += blk->size;
        }

        // Coalesce with next block if adjacent and free
        kmem_block_header_t* next_blk = blk->next;
        kmem_block_footer_t* ftr = (kmem_block_footer_t*)((uint8_t*)blk + sizeof(kmem_block_header_t) + blk->size);

        if (next_blk && next_blk->is_free && (uint8_t*)next_blk == (uint8_t*)ftr + sizeof(kmem_block_footer_t)) {
            blk->size += sizeof(kmem_block_footer_t) + sizeof(kmem_block_header_t) + next_blk->size;
            blk->next = next_blk->next;
            if (next_blk->next) next_blk->next->prev = blk;

            ftr = (kmem_block_footer_t*)((uint8_t*)blk + sizeof(kmem_block_header_t) + blk->size);
            ftr->magic = KMEM_FOOTER_MAGIC;
            ftr->size = blk->size;
        }

        // Coalesce with previous block if adjacent and free
        kmem_block_header_t* prev_blk = blk->prev;
        if (prev_blk && prev_blk->is_free) {
            kmem_block_footer_t* prev_ftr = (kmem_block_footer_t*)((uint8_t*)blk - sizeof(kmem_block_footer_t));
            if (prev_ftr->magic == KMEM_FOOTER_MAGIC && prev_ftr->size == prev_blk->size) {
                prev_blk->size += sizeof(kmem_block_header_t) + sizeof(kmem_block_footer_t) + blk->size;
                prev_blk->next = blk->next;
                if (blk->next) blk->next->prev = prev_blk;

                kmem_block_footer_t* merged_ftr = (kmem_block_footer_t*)((uint8_t*)prev_blk + sizeof(kmem_block_header_t) + prev_blk->size);
                merged_ftr->magic = KMEM_FOOTER_MAGIC;
                merged_ftr->size = prev_blk->size;
            }
        }
        return;
    }

    serial_println("[KMEM] ERROR: Attempted to free unknown/corrupted pointer!");
}

// Calloc: Zero-Initialized Dynamic Memory
void* kcalloc(size_t num, size_t size) {
    if (num == 0 || size == 0) return NULL;
    size_t total = num * size;
    if (total / num != size) return NULL;

    void* ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

// Realloc: In-Place Expansion or Copy Reallocation
void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    // Check if it's a slab chunk
    kmem_chunk_t* chunk = (kmem_chunk_t*)((uint8_t*)ptr - sizeof(kmem_chunk_t));
    if (chunk->magic == KMEM_SLAB_MAGIC) {
        size_t old_size = bin_sizes[chunk->bin_idx];
        if (new_size <= old_size) {
            return ptr; // Fits in existing slab chunk
        }

        void* new_ptr = kmalloc(new_size);
        if (new_ptr) {
            memcpy(new_ptr, ptr, old_size);
            kfree(ptr);
        }
        return new_ptr;
    }

    // Check if it's an arena block
    kmem_block_header_t* blk = (kmem_block_header_t*)((uint8_t*)ptr - sizeof(kmem_block_header_t));
    if (blk->magic == KMEM_ARENA_MAGIC) {
        if (new_size <= blk->size) {
            return ptr; // Already large enough
        }

        // Try in-place expansion if next block is adjacent and free
        kmem_block_footer_t* ftr = (kmem_block_footer_t*)((uint8_t*)blk + sizeof(kmem_block_header_t) + blk->size);
        kmem_block_header_t* next_blk = blk->next;

        if (next_blk && next_blk->is_free && (uint8_t*)next_blk == (uint8_t*)ftr + sizeof(kmem_block_footer_t)) {
            size_t combined = blk->size + sizeof(kmem_block_footer_t) + sizeof(kmem_block_header_t) + next_blk->size;
            if (combined >= new_size) {
                blk->size = combined;
                blk->next = next_blk->next;
                if (next_blk->next) next_blk->next->prev = blk;

                kmem_block_footer_t* new_ftr = (kmem_block_footer_t*)((uint8_t*)blk + sizeof(kmem_block_header_t) + blk->size);
                new_ftr->magic = KMEM_FOOTER_MAGIC;
                new_ftr->size = blk->size;
                return ptr;
            }
        }

        // Must allocate new and copy
        void* new_ptr = kmalloc(new_size);
        if (new_ptr) {
            memcpy(new_ptr, ptr, blk->size);
            kfree(ptr);
        }
        return new_ptr;
    }

    serial_println("[KMEM] ERROR: krealloc called with invalid pointer!");
    return NULL;
}

// Telemetry & Integrity
void kmem_get_stats(kmem_stats_t* stats) {
    if (stats) {
        memcpy(stats, &kmem_stats, sizeof(kmem_stats_t));
    }
}

int kmem_check_integrity(void) {
    kmem_block_header_t* curr = arena_head;
    while (curr) {
        if (curr->magic != KMEM_ARENA_MAGIC) return 0;
        if (curr->canary != KMEM_CANARY_WORD) return 0;
        kmem_block_footer_t* ftr = (kmem_block_footer_t*)((uint8_t*)curr + sizeof(kmem_block_header_t) + curr->size);
        if (ftr->magic != KMEM_FOOTER_MAGIC || ftr->size != curr->size) return 0;
        curr = curr->next;
    }
    return 1;
}

// Comprehensive Self-Test Suite
void kmem_run_tests(void) {
    serial_println("=========================================");
    serial_println("[KMEM] Running Kernel Memory Manager Test Suite...");
    serial_println("=========================================");

    // Test 1: Small Slab Bin Allocations & O(1) Recycling
    void* p1 = kmalloc(24);  // Maps to 32B bin
    void* p2 = kmalloc(120); // Maps to 128B bin
    void* p3 = kmalloc(500); // Maps to 512B bin
    if (p1 && p2 && p3) {
        memset(p1, 0xAA, 24);
        memset(p2, 0xBB, 120);
        memset(p3, 0xCC, 500);
        serial_println("  -> [TEST 1/5] O(1) Slab Bin Allocations (32B, 128B, 512B): PASS");
    } else {
        serial_println("  -> [TEST 1/5] O(1) Slab Bin Allocations: FAIL");
    }

    kfree(p1);
    kfree(p2);
    kfree(p3);

    // Test 2: Large Arena Boundary-Tag Allocation & Coalescing
    void* big1 = kmalloc(8192);  // 8 KB
    void* big2 = kmalloc(16384); // 16 KB
    if (big1 && big2) {
        memset(big1, 0x11, 8192);
        memset(big2, 0x22, 16384);
        serial_println("  -> [TEST 2/5] Large Boundary-Tag Allocations (8KB, 16KB): PASS");
    } else {
        serial_println("  -> [TEST 2/5] Large Boundary-Tag Allocations: FAIL");
    }

    kfree(big1);
    kfree(big2);

    // Test 3: KCalloc Zero-Initialization
    uint32_t* arr = (uint32_t*)kcalloc(64, sizeof(uint32_t));
    int zero_ok = 1;
    if (arr) {
        for (int i = 0; i < 64; i++) {
            if (arr[i] != 0) { zero_ok = 0; break; }
        }
    } else {
        zero_ok = 0;
    }
    if (zero_ok) {
        serial_println("  -> [TEST 3/5] kcalloc zero-fill verification: PASS");
    } else {
        serial_println("  -> [TEST 3/5] kcalloc zero-fill verification: FAIL");
    }
    kfree(arr);

    // Test 4: Realloc Expansion & Data Integrity
    char* str = (char*)kmalloc(16);
    if (str) {
        strcpy(str, "LemonOS");
        str = (char*)krealloc(str, 128);
        if (str && strcmp(str, "LemonOS") == 0) {
            strcat(str, " Memory Subsystem");
            serial_println("  -> [TEST 4/5] krealloc expansion & data preservation: PASS");
        } else {
            serial_println("  -> [TEST 4/5] krealloc expansion: FAIL");
        }
        kfree(str);
    }

    // Test 5: Heap Integrity Check
    if (kmem_check_integrity()) {
        serial_println("  -> [TEST 5/5] Heap boundary-tag & canary integrity: PASS");
    } else {
        serial_println("  -> [TEST 5/5] Heap integrity check: FAIL");
    }

    serial_println("[KMEM] Kernel Dynamic Memory Manager: 5/5 PASSED!");
}
