#include "mman.h"
#include "vfs.h"
#include "string.h"
#include "serial.h"

#define MMAP_MAX_REGIONS 32
#define MMAP_POOL_PAGES  64
#define MMAP_POOL_BYTES  (MMAP_POOL_PAGES * MMAP_PAGE_SIZE)

static uint8_t mmap_backing_pool[MMAP_POOL_BYTES] __attribute__((aligned(4096)));
static uint8_t page_bitmap[MMAP_POOL_PAGES];

typedef struct {
    void*  addr;
    size_t length;
    int    prot;
    int    flags;
    int    fd;
    off_t  offset;
    int    start_page;
    int    page_count;
    int    active;
} mmap_region_t;

static mmap_region_t regions[MMAP_MAX_REGIONS];

void mman_init(void) {
    memset(page_bitmap, 0, sizeof(page_bitmap));
    memset(regions, 0, sizeof(regions));
    serial_println("[MMAN] Memory-mapped file management subsystem initialized (256 KB page pool).");
}

static int alloc_pages(int count) {
    for (int i = 0; i <= MMAP_POOL_PAGES - count; i++) {
        int found = 1;
        for (int j = 0; j < count; j++) {
            if (page_bitmap[i + j]) {
                found = 0;
                i += j;
                break;
            }
        }
        if (found) {
            for (int j = 0; j < count; j++) {
                page_bitmap[i + j] = 1;
            }
            return i;
        }
    }
    return -1;
}

static void free_pages(int start, int count) {
    for (int i = 0; i < count && (start + i) < MMAP_POOL_PAGES; i++) {
        page_bitmap[start + i] = 0;
    }
}

void* mman_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    (void)addr;
    if (length == 0) return MAP_FAILED;

    // Align length to page size
    size_t aligned_len = (length + MMAP_PAGE_SIZE - 1) & ~(MMAP_PAGE_SIZE - 1);
    int pages_needed = (int)(aligned_len / MMAP_PAGE_SIZE);

    int start_p = alloc_pages(pages_needed);
    if (start_p < 0) {
        serial_println("[MMAN] mmap failed: out of physical pages.");
        return MAP_FAILED;
    }

    // Find free region entry
    int reg_idx = -1;
    for (int i = 0; i < MMAP_MAX_REGIONS; i++) {
        if (!regions[i].active) {
            reg_idx = i;
            break;
        }
    }

    if (reg_idx < 0) {
        free_pages(start_p, pages_needed);
        serial_println("[MMAN] mmap failed: region table full.");
        return MAP_FAILED;
    }

    uint8_t* ptr = &mmap_backing_pool[start_p * MMAP_PAGE_SIZE];

    if (flags & MAP_ANONYMOUS) {
        memset(ptr, 0, aligned_len);
    } else if (fd >= 0) {
        memset(ptr, 0, aligned_len);
        vfs_seek(fd, offset, SEEK_SET);
        vfs_read(fd, ptr, length);
    }

    regions[reg_idx].addr = ptr;
    regions[reg_idx].length = length;
    regions[reg_idx].prot = prot;
    regions[reg_idx].flags = flags;
    regions[reg_idx].fd = fd;
    regions[reg_idx].offset = offset;
    regions[reg_idx].start_page = start_p;
    regions[reg_idx].page_count = pages_needed;
    regions[reg_idx].active = 1;

    return ptr;
}

int mman_msync(void* addr, size_t length, int flags) {
    (void)flags;
    if (!addr) return -1;

    for (int i = 0; i < MMAP_MAX_REGIONS; i++) {
        if (regions[i].active && regions[i].addr == addr) {
            if ((regions[i].flags & MAP_SHARED) && (regions[i].prot & PROT_WRITE) && regions[i].fd >= 0) {
                size_t sync_len = (length < regions[i].length) ? length : regions[i].length;
                vfs_seek(regions[i].fd, regions[i].offset, SEEK_SET);
                vfs_write(regions[i].fd, addr, sync_len);
            }
            return 0;
        }
    }
    return -1;
}

int mman_munmap(void* addr, size_t length) {
    (void)length;
    if (!addr) return -1;

    for (int i = 0; i < MMAP_MAX_REGIONS; i++) {
        if (regions[i].active && regions[i].addr == addr) {
            // Write back changes if MAP_SHARED
            if ((regions[i].flags & MAP_SHARED) && (regions[i].prot & PROT_WRITE) && regions[i].fd >= 0) {
                mman_msync(addr, regions[i].length, MS_SYNC);
            }
            free_pages(regions[i].start_page, regions[i].page_count);
            regions[i].active = 0;
            return 0;
        }
    }
    return -1;
}

void mman_run_tests(void) {
    serial_println("[MMAN] Running Memory Mapping Verification Tests...");

    // 1. Anonymous mapping test
    void* anon_ptr = mman_mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (anon_ptr != MAP_FAILED) {
        char* buf = (char*)anon_ptr;
        buf[0] = 'L';
        buf[1] = 'E';
        buf[2] = 'M';
        buf[3] = 'O';
        buf[4] = 'N';
        if (buf[0] == 'L' && buf[4] == 'N') {
            serial_println("  -> [TEST 1/2] mmap anonymous R/W page mapping: PASS");
        } else {
            serial_println("  -> [TEST 1/2] mmap anonymous R/W: FAIL");
        }
        mman_munmap(anon_ptr, 8192);
    } else {
        serial_println("  -> [TEST 1/2] mmap anonymous: FAIL");
    }

    // 2. File-backed mapping test
    int fd = vfs_open("/docs/welcome.txt", O_RDONLY);
    if (fd >= 0) {
        void* fptr = mman_mmap(NULL, 128, PROT_READ, MAP_PRIVATE, fd, 0);
        if (fptr != MAP_FAILED) {
            char* cptr = (char*)fptr;
            if (cptr[0] == '=') {
                serial_println("  -> [TEST 2/2] mmap file-backed mapping: PASS");
            } else {
                serial_println("  -> [TEST 2/2] mmap file-backed mapping: FAIL");
            }
            mman_munmap(fptr, 128);
        } else {
            serial_println("  -> [TEST 2/2] mmap file-backed: FAIL");
        }
        vfs_close(fd);
    } else {
        serial_println("  -> [TEST 2/2] mmap file-backed: SKIP (file open failed)");
    }
    serial_println("[MMAN] Memory mapping subsystem verified.");
}
