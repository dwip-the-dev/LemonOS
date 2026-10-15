#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <stddef.h>

#define BLOCK_SECTOR_SIZE   512
#define BLOCK_MAX_DEVICES   8
#define BLOCK_NAME_MAX      32

struct block_dev;
typedef struct block_dev block_dev_t;

typedef int (*block_read_fn)(block_dev_t* dev, uint32_t lba, uint32_t count, void* buf);
typedef int (*block_write_fn)(block_dev_t* dev, uint32_t lba, uint32_t count, const void* buf);
typedef int (*block_flush_fn)(block_dev_t* dev);

struct block_dev {
    char            name[BLOCK_NAME_MAX];
    uint32_t        sector_size;
    uint32_t        total_sectors;
    uint32_t        flags;
    void*           priv_data;

    block_read_fn   read_sectors;
    block_write_fn  write_sectors;
    block_flush_fn  flush;
};

// Flags
#define BLOCK_FLAG_READONLY   0x01
#define BLOCK_FLAG_REMOVABLE  0x02
#define BLOCK_FLAG_RAMDISK    0x04
#define BLOCK_FLAG_IDE        0x08

// Block Device Management API
void          block_init(void);
int           block_register_device(block_dev_t* dev);
block_dev_t*  block_get_device(const char* name);
block_dev_t*  block_get_default(void);
int           block_get_device_count(void);
block_dev_t*  block_get_device_by_index(int index);

// Block I/O Helper Functions
int           block_read(block_dev_t* dev, uint32_t lba, uint32_t count, void* buf);
int           block_write(block_dev_t* dev, uint32_t lba, uint32_t count, const void* buf);
int           block_flush(block_dev_t* dev);

// Verification Suite
void          block_run_tests(void);

#endif
