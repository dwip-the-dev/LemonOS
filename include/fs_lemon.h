#ifndef FS_LEMON_H
#define FS_LEMON_H

#include "block.h"
#include "vfs.h"
#include <stdint.h>
#include <stddef.h>

#define LEMON_FS_MAGIC          0x4C454D4E  // "LEMN"
#define LEMON_FS_VERSION        1
#define LEMON_BLOCK_SIZE        512
#define LEMON_DIRECT_BLOCKS     10
#define LEMON_INODES_PER_BLOCK  8           // 512 / 64 bytes
#define LEMON_DIRENTS_PER_BLOCK 8           // 512 / 64 bytes
#define LEMON_NAME_MAX          58

// On-disk Superblock (Sector 1)
typedef struct {
    uint32_t magic;                 // LEMON_FS_MAGIC
    uint32_t version;               // LEMON_FS_VERSION
    uint32_t block_size;            // 512
    uint32_t total_blocks;          // Total blocks on device
    uint32_t free_blocks;           // Free data blocks
    uint32_t total_inodes;          // Total inodes (e.g. 128)
    uint32_t free_inodes;           // Free inodes
    uint32_t block_bitmap_lba;      // Starting sector for block bitmap
    uint32_t block_bitmap_sectors;  // Number of sectors for block bitmap
    uint32_t inode_bitmap_lba;      // Starting sector for inode bitmap
    uint32_t inode_bitmap_sectors;  // Number of sectors for inode bitmap
    uint32_t inode_table_lba;       // Starting sector for inode table
    uint32_t inode_table_sectors;   // Number of sectors for inode table
    uint32_t data_blocks_lba;       // Starting sector for data blocks
    uint32_t root_inode;            // Inode 1
    uint8_t  padding[452];          // Pad to 512 bytes
} __attribute__((packed)) lemon_super_t;

// On-disk Inode (64 bytes)
typedef struct {
    uint16_t type;                              // VFS_FILE or VFS_DIRECTORY
    uint16_t permissions;                       // e.g. 0755, 0644
    uint32_t size;                              // Size in bytes
    uint32_t blocks;                            // Allocated block count
    uint32_t direct_blocks[LEMON_DIRECT_BLOCKS];// Direct LBA pointers (10 * 512 = 5120 bytes)
    uint32_t indirect_block;                    // Single indirect block LBA (128 * 512 = 65536 bytes)
    uint16_t uid;                               // User ID
    uint16_t gid;                               // Group ID
    uint32_t mtime;                             // Modification timestamp
} __attribute__((packed)) lemon_inode_t;

// On-disk Directory Entry (64 bytes)
typedef struct {
    uint32_t inode;                             // Inode number (0 = unused)
    uint8_t  type;                              // VFS_FILE or VFS_DIRECTORY
    char     name[LEMON_NAME_MAX + 1];          // Null-terminated filename (59 bytes)
} __attribute__((packed)) lemon_dirent_t;

// LemonFS Driver API
int         fs_lemon_format(block_dev_t* dev);
vfs_node_t* fs_lemon_mount(block_dev_t* dev);
void        fs_lemon_run_tests(block_dev_t* dev);

#endif
