#ifndef BCACHE_H
#define BCACHE_H

#include <stdint.h>
#include <stddef.h>
#include "block.h"

// LemonOS Kernel High-Performance Block Buffer Cache (bcache)
#define BCACHE_NUM_BUFFERS 128      // 128 sectors = 64 KB cached in RAM
#define BCACHE_HASH_SLOTS  64       // 64 hash buckets for O(1) buffer lookup

#define BCACHE_FLAG_FREE   0x00
#define BCACHE_FLAG_VALID  0x01
#define BCACHE_FLAG_DIRTY  0x02
#define BCACHE_FLAG_BUSY   0x04

typedef struct bcache_buf {
    block_dev_t*       dev;
    uint32_t           lba;
    uint8_t            data[BLOCK_SECTOR_SIZE];
    uint8_t            flags;
    uint32_t           access_count;
    struct bcache_buf* next_hash;
    struct bcache_buf* prev_lru;
    struct bcache_buf* next_lru;
} bcache_buf_t;

typedef struct {
    uint32_t total_reads;
    uint32_t total_writes;
    uint32_t cache_hits;
    uint32_t cache_misses;
    uint32_t writebacks;
    uint32_t dirty_count;
    uint32_t cached_buffers;
    uint32_t hit_ratio_pct;
} bcache_stats_t;

// Public Buffer Cache API
void bcache_init(void);
int  bcache_read(block_dev_t* dev, uint32_t lba, void* out_buf);
int  bcache_write(block_dev_t* dev, uint32_t lba, const void* in_buf);
int  bcache_sync(block_dev_t* dev);
void bcache_get_stats(bcache_stats_t* out_stats);
void bcache_dump_stats(void);
void bcache_run_tests(void);

#endif // BCACHE_H
