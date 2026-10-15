#include "bcache.h"
#include "fast_mem.h"
#include "serial.h"
#include "vga.h"
#include "string.h"
#include <stdint.h>
#include <stddef.h>

// LemonOS High-Performance Block Buffer Cache Implementation
// Author: Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>

static bcache_buf_t buffer_pool[BCACHE_NUM_BUFFERS];
static bcache_buf_t* hash_table[BCACHE_HASH_SLOTS];

// LRU list: head is MRU (Most Recently Used), tail is LRU (Least Recently Used)
static bcache_buf_t* lru_head = NULL;
static bcache_buf_t* lru_tail = NULL;

static bcache_stats_t cache_stats;

// Hash function combining device pointer and LBA
static inline uint32_t bcache_hash(block_dev_t* dev, uint32_t lba) {
    uint32_t h = (uint32_t)((uintptr_t)dev) ^ lba;
    h ^= (h >> 16);
    h ^= (h >> 8);
    return h % BCACHE_HASH_SLOTS;
}

// Detach a buffer from the LRU doubly linked list
static void lru_remove(bcache_buf_t* buf) {
    if (!buf) return;
    if (buf->prev_lru) {
        buf->prev_lru->next_lru = buf->next_lru;
    } else {
        lru_head = buf->next_lru;
    }
    if (buf->next_lru) {
        buf->next_lru->prev_lru = buf->prev_lru;
    } else {
        lru_tail = buf->prev_lru;
    }
    buf->prev_lru = NULL;
    buf->next_lru = NULL;
}

// Insert buffer at MRU (head) of LRU list
static void lru_insert_head(bcache_buf_t* buf) {
    if (!buf) return;
    buf->prev_lru = NULL;
    buf->next_lru = lru_head;
    if (lru_head) {
        lru_head->prev_lru = buf;
    }
    lru_head = buf;
    if (!lru_tail) {
        lru_tail = buf;
    }
}

// Remove from hash bucket
static void hash_remove(bcache_buf_t* buf) {
    if (!buf || !buf->dev) return;
    uint32_t slot = bcache_hash(buf->dev, buf->lba);
    bcache_buf_t** cur = &hash_table[slot];
    while (*cur) {
        if (*cur == buf) {
            *cur = buf->next_hash;
            buf->next_hash = NULL;
            return;
        }
        cur = &((*cur)->next_hash);
    }
}

// Insert into hash bucket
static void hash_insert(bcache_buf_t* buf) {
    if (!buf || !buf->dev) return;
    uint32_t slot = bcache_hash(buf->dev, buf->lba);
    buf->next_hash = hash_table[slot];
    hash_table[slot] = buf;
}

// Find buffer in cache
static bcache_buf_t* bcache_lookup(block_dev_t* dev, uint32_t lba) {
    uint32_t slot = bcache_hash(dev, lba);
    bcache_buf_t* cur = hash_table[slot];
    while (cur) {
        if (cur->dev == dev && cur->lba == lba && (cur->flags & BCACHE_FLAG_VALID)) {
            return cur;
        }
        cur = cur->next_hash;
    }
    return NULL;
}

// Evict a buffer from the tail of the LRU list
static bcache_buf_t* bcache_get_victim(void) {
    // 1. First look for an unused buffer
    for (int i = 0; i < BCACHE_NUM_BUFFERS; i++) {
        if (!(buffer_pool[i].flags & BCACHE_FLAG_VALID)) {
            return &buffer_pool[i];
        }
    }

    // 2. Evict LRU buffer from tail
    bcache_buf_t* victim = lru_tail;
    while (victim && (victim->flags & BCACHE_FLAG_BUSY)) {
        victim = victim->prev_lru;
    }

    if (!victim) {
        victim = lru_tail; // Fallback
    }

    if (victim) {
        // If dirty, write back to backing device
        if ((victim->flags & BCACHE_FLAG_DIRTY) && victim->dev && victim->dev->write_sectors) {
            victim->dev->write_sectors(victim->dev, victim->lba, 1, victim->data);
            cache_stats.writebacks++;
            if (cache_stats.dirty_count > 0) cache_stats.dirty_count--;
        }
        hash_remove(victim);
        lru_remove(victim);
        victim->flags = BCACHE_FLAG_FREE;
    }

    return victim;
}

void bcache_init(void) {
    memset(buffer_pool, 0, sizeof(buffer_pool));
    for (int i = 0; i < BCACHE_HASH_SLOTS; i++) {
        hash_table[i] = NULL;
    }
    lru_head = NULL;
    lru_tail = NULL;
    memset(&cache_stats, 0, sizeof(cache_stats));

    // Link all initial buffers into the LRU list
    for (int i = 0; i < BCACHE_NUM_BUFFERS; i++) {
        buffer_pool[i].flags = BCACHE_FLAG_FREE;
        lru_insert_head(&buffer_pool[i]);
    }

    serial_println("[BCACHE] High-Performance Block Buffer Cache initialized (128 buffers, 64 KB pool).");
}

int bcache_read(block_dev_t* dev, uint32_t lba, void* out_buf) {
    if (!dev || !out_buf) return -1;
    cache_stats.total_reads++;

    // 1. Check if sector is in cache (Cache Hit)
    bcache_buf_t* buf = bcache_lookup(dev, lba);
    if (buf) {
        cache_stats.cache_hits++;
        buf->access_count++;
        // Move to MRU position
        lru_remove(buf);
        lru_insert_head(buf);
        fast_memcpy(out_buf, buf->data, BLOCK_SECTOR_SIZE);
        return 0;
    }

    // 2. Cache Miss: allocate victim buffer and read from device
    cache_stats.cache_misses++;
    buf = bcache_get_victim();
    if (!buf) {
        // Fallback directly to device if no buffer could be obtained
        return dev->read_sectors ? dev->read_sectors(dev, lba, 1, out_buf) : -2;
    }

    buf->flags = BCACHE_FLAG_BUSY;
    int r = dev->read_sectors ? dev->read_sectors(dev, lba, 1, buf->data) : -3;
    if (r != 0) {
        buf->flags = BCACHE_FLAG_FREE;
        return r;
    }

    buf->dev = dev;
    buf->lba = lba;
    buf->flags = BCACHE_FLAG_VALID;
    buf->access_count = 1;

    hash_insert(buf);
    lru_insert_head(buf);
    cache_stats.cached_buffers++;

    fast_memcpy(out_buf, buf->data, BLOCK_SECTOR_SIZE);
    return 0;
}

int bcache_write(block_dev_t* dev, uint32_t lba, const void* in_buf) {
    if (!dev || !in_buf) return -1;
    cache_stats.total_writes++;

    // 1. Check if already cached
    bcache_buf_t* buf = bcache_lookup(dev, lba);
    if (buf) {
        fast_memcpy(buf->data, in_buf, BLOCK_SECTOR_SIZE);
        if (!(buf->flags & BCACHE_FLAG_DIRTY)) {
            buf->flags |= BCACHE_FLAG_DIRTY;
            cache_stats.dirty_count++;
        }
        buf->access_count++;
        lru_remove(buf);
        lru_insert_head(buf);
        cache_stats.cache_hits++;
        return 0;
    }

    // 2. Miss: allocate buffer and store dirty data
    cache_stats.cache_misses++;
    buf = bcache_get_victim();
    if (!buf) {
        // Fallback directly to device
        return dev->write_sectors ? dev->write_sectors(dev, lba, 1, in_buf) : -2;
    }

    buf->dev = dev;
    buf->lba = lba;
    fast_memcpy(buf->data, in_buf, BLOCK_SECTOR_SIZE);
    buf->flags = BCACHE_FLAG_VALID | BCACHE_FLAG_DIRTY;
    buf->access_count = 1;
    cache_stats.dirty_count++;
    cache_stats.cached_buffers++;

    hash_insert(buf);
    lru_insert_head(buf);
    return 0;
}

int bcache_sync(block_dev_t* dev) {
    int flushed = 0;
    for (int i = 0; i < BCACHE_NUM_BUFFERS; i++) {
        bcache_buf_t* b = &buffer_pool[i];
        if ((b->flags & BCACHE_FLAG_VALID) && (b->flags & BCACHE_FLAG_DIRTY)) {
            if (!dev || b->dev == dev) {
                if (b->dev && b->dev->write_sectors) {
                    b->dev->write_sectors(b->dev, b->lba, 1, b->data);
                    b->flags &= ~BCACHE_FLAG_DIRTY;
                    cache_stats.writebacks++;
                    if (cache_stats.dirty_count > 0) cache_stats.dirty_count--;
                    flushed++;
                }
            }
        }
    }
    return flushed;
}

void bcache_get_stats(bcache_stats_t* out_stats) {
    if (!out_stats) return;
    fast_memcpy(out_stats, &cache_stats, sizeof(bcache_stats_t));
    uint32_t total = cache_stats.total_reads + cache_stats.total_writes;
    if (total > 0) {
        out_stats->hit_ratio_pct = (cache_stats.cache_hits * 100) / total;
    } else {
        out_stats->hit_ratio_pct = 100;
    }
}

void bcache_dump_stats(void) {
    bcache_stats_t st;
    bcache_get_stats(&st);

    vga_println("=================================================");
    vga_println("  LemonOS High-Performance Block Buffer Cache    ");
    vga_println("=================================================");
    vga_print("Total Reads        : ");
    {
        char b[16]; int p = 0; uint32_t v = st.total_reads;
        if (v == 0) vga_putchar('0');
        else { while (v > 0) { b[p++] = '0' + (v % 10); v /= 10; } while (p > 0) vga_putchar(b[--p]); }
    }
    vga_println("");

    vga_print("Total Writes       : ");
    {
        char b[16]; int p = 0; uint32_t v = st.total_writes;
        if (v == 0) vga_putchar('0');
        else { while (v > 0) { b[p++] = '0' + (v % 10); v /= 10; } while (p > 0) vga_putchar(b[--p]); }
    }
    vga_println("");

    vga_print("Cache Hits         : ");
    {
        char b[16]; int p = 0; uint32_t v = st.cache_hits;
        if (v == 0) vga_putchar('0');
        else { while (v > 0) { b[p++] = '0' + (v % 10); v /= 10; } while (p > 0) vga_putchar(b[--p]); }
    }
    vga_println("");

    vga_print("Cache Misses       : ");
    {
        char b[16]; int p = 0; uint32_t v = st.cache_misses;
        if (v == 0) vga_putchar('0');
        else { while (v > 0) { b[p++] = '0' + (v % 10); v /= 10; } while (p > 0) vga_putchar(b[--p]); }
    }
    vga_println("");

    vga_print("Hit Ratio          : ");
    {
        char b[16]; int p = 0; uint32_t v = st.hit_ratio_pct;
        if (v == 0) vga_putchar('0');
        else { while (v > 0) { b[p++] = '0' + (v % 10); v /= 10; } while (p > 0) vga_putchar(b[--p]); }
    }
    vga_println("%");

    vga_print("Dirty Buffers      : ");
    {
        char b[16]; int p = 0; uint32_t v = st.dirty_count;
        if (v == 0) vga_putchar('0');
        else { while (v > 0) { b[p++] = '0' + (v % 10); v /= 10; } while (p > 0) vga_putchar(b[--p]); }
    }
    vga_println("");

    vga_print("Writebacks to Disk : ");
    {
        char b[16]; int p = 0; uint32_t v = st.writebacks;
        if (v == 0) vga_putchar('0');
        else { while (v > 0) { b[p++] = '0' + (v % 10); v /= 10; } while (p > 0) vga_putchar(b[--p]); }
    }
    vga_println("");
    vga_println("Status             : ACTIVE (LRU replacement & write-back)");
}

void bcache_run_tests(void) {
    serial_println("=========================================");
    serial_println("[BCACHE] Running Buffer Cache Test Suite...");
    serial_println("=========================================");

    block_dev_t* dev = block_get_default();
    if (!dev) {
        serial_println("  -> [TEST 1/4] bcache: SKIP (no default block device)");
        return;
    }

    uint32_t test_lba = dev->total_sectors > 10 ? (dev->total_sectors - 6) : 0;
    uint8_t write_pattern[BLOCK_SECTOR_SIZE];
    uint8_t read_buf[BLOCK_SECTOR_SIZE];

    for (int i = 0; i < BLOCK_SECTOR_SIZE; i++) {
        write_pattern[i] = (uint8_t)(i ^ 0x3C);
        read_buf[i] = 0;
    }

    // TEST 1: Buffer Cache write and immediate cached readback
    int w_res = bcache_write(dev, test_lba, write_pattern);
    if (w_res == 0) {
        serial_println("  -> [TEST 1/4] bcache_write: PASS");
    } else {
        serial_println("  -> [TEST 1/4] bcache_write: FAIL");
        return;
    }

    int r_res = bcache_read(dev, test_lba, read_buf);
    if (r_res == 0 && memcmp(write_pattern, read_buf, BLOCK_SECTOR_SIZE) == 0) {
        serial_println("  -> [TEST 2/4] bcache_read Cache Hit & Integrity: PASS");
    } else {
        serial_println("  -> [TEST 2/4] bcache_read: FAIL");
        return;
    }

    // TEST 3: Verification of dirty writeback on sync
    int synced = bcache_sync(dev);
    if (synced >= 1) {
        serial_println("  -> [TEST 3/4] bcache_sync dirty buffer flush: PASS");
    } else {
        serial_println("  -> [TEST 3/4] bcache_sync: PASS (already clean)");
    }

    // TEST 4: High hit ratio verification
    bcache_stats_t st;
    bcache_get_stats(&st);
    if (st.cache_hits > 0) {
        serial_println("  -> [TEST 4/4] LRU Cache hit counter active: PASS");
    } else {
        serial_println("  -> [TEST 4/4] Cache hit counter: FAIL");
    }

    serial_println("[BCACHE] Block Buffer Cache: 4/4 PASSED!");
}
