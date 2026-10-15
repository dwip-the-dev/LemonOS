#include "fs_lemon.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

#define LEMON_NODE_POOL_SIZE 64
#define LEMON_TOTAL_INODES   256

static vfs_node_t node_pool[LEMON_NODE_POOL_SIZE];
static int        node_pool_used[LEMON_NODE_POOL_SIZE];

static int lemon_strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static void lemon_strcpy(char* d, const char* s, int max) {
    int i = 0;
    while (s && s[i] && i < max - 1) { d[i] = s[i]; i++; }
    d[i] = '\0';
}

static __attribute__((unused)) int lemon_strlen(const char* s) {
    int len = 0;
    while (s && s[len]) len++;
    return len;
}

// VFS Node Pool
static vfs_node_t* alloc_vfs_node(void) {
    for (int i = 0; i < LEMON_NODE_POOL_SIZE; i++) {
        if (!node_pool_used[i]) {
            node_pool_used[i] = 1;
            return &node_pool[i];
        }
    }
    // Recycle slot 1..N (never recycle slot 0 if it's root)
    static int next_victim = 1;
    int idx = next_victim++;
    if (next_victim >= LEMON_NODE_POOL_SIZE) next_victim = 1;
    return &node_pool[idx];
}

// Low-level Superblock & Inode Helpers
static int read_super(block_dev_t* dev, lemon_super_t* out_sb) {
    if (!dev || !out_sb) return -1;
    uint8_t buf[BLOCK_SECTOR_SIZE];
    if (block_read(dev, 1, 1, buf) < 0) return -1;

    lemon_super_t* sb = (lemon_super_t*)buf;
    if (sb->magic != LEMON_FS_MAGIC) return -2;

    *out_sb = *sb;
    return 0;
}

static int write_super(block_dev_t* dev, const lemon_super_t* sb) {
    if (!dev || !sb) return -1;
    return block_write(dev, 1, 1, sb);
}

static int read_inode(block_dev_t* dev, const lemon_super_t* sb, uint32_t ino, lemon_inode_t* out_ino) {
    if (!dev || !sb || !out_ino || ino == 0 || ino > sb->total_inodes) return -1;

    uint32_t ino_idx = ino - 1;
    uint32_t lba = sb->inode_table_lba + (ino_idx / LEMON_INODES_PER_BLOCK);
    uint32_t off = (ino_idx % LEMON_INODES_PER_BLOCK) * sizeof(lemon_inode_t);

    uint8_t buf[BLOCK_SECTOR_SIZE];
    if (block_read(dev, lba, 1, buf) < 0) return -2;

    lemon_inode_t* table = (lemon_inode_t*)buf;
    *out_ino = table[ino_idx % LEMON_INODES_PER_BLOCK];
    (void)off;
    return 0;
}

static int write_inode(block_dev_t* dev, const lemon_super_t* sb, uint32_t ino, const lemon_inode_t* in_ino) {
    if (!dev || !sb || !in_ino || ino == 0 || ino > sb->total_inodes) return -1;

    uint32_t ino_idx = ino - 1;
    uint32_t lba = sb->inode_table_lba + (ino_idx / LEMON_INODES_PER_BLOCK);

    uint8_t buf[BLOCK_SECTOR_SIZE];
    if (block_read(dev, lba, 1, buf) < 0) return -2;

    lemon_inode_t* table = (lemon_inode_t*)buf;
    table[ino_idx % LEMON_INODES_PER_BLOCK] = *in_ino;

    return block_write(dev, lba, 1, buf);
}

// Bitmap Allocators
static uint32_t alloc_block(block_dev_t* dev, lemon_super_t* sb) {
    if (sb->free_blocks == 0) return 0;

    uint8_t buf[BLOCK_SECTOR_SIZE];
    for (uint32_t s = 0; s < sb->block_bitmap_sectors; s++) {
        uint32_t lba = sb->block_bitmap_lba + s;
        if (block_read(dev, lba, 1, buf) < 0) return 0;

        for (uint32_t byte_idx = 0; byte_idx < BLOCK_SECTOR_SIZE; byte_idx++) {
            if (buf[byte_idx] != 0xFF) {
                for (int bit = 0; bit < 8; bit++) {
                    if (!(buf[byte_idx] & (1 << bit))) {
                        uint32_t block_num = (s * BLOCK_SECTOR_SIZE + byte_idx) * 8 + bit;
                        if (block_num >= sb->total_blocks) return 0;

                        buf[byte_idx] |= (1 << bit);
                        if (block_write(dev, lba, 1, buf) < 0) return 0;

                        sb->free_blocks--;
                        write_super(dev, sb);

                        // Zero out the newly allocated sector
                        uint8_t zero_buf[BLOCK_SECTOR_SIZE];
                        for (int z = 0; z < BLOCK_SECTOR_SIZE; z++) zero_buf[z] = 0;
                        block_write(dev, block_num, 1, zero_buf);

                        return block_num;
                    }
                }
            }
        }
    }
    return 0;
}

static void free_block(block_dev_t* dev, lemon_super_t* sb, uint32_t block_lba) {
    if (block_lba < sb->data_blocks_lba || block_lba >= sb->total_blocks) return;

    uint32_t sec_idx = block_lba / (BLOCK_SECTOR_SIZE * 8);
    uint32_t rem = block_lba % (BLOCK_SECTOR_SIZE * 8);
    uint32_t byte_idx = rem / 8;
    uint32_t bit = rem % 8;

    uint8_t buf[BLOCK_SECTOR_SIZE];
    uint32_t lba = sb->block_bitmap_lba + sec_idx;
    if (block_read(dev, lba, 1, buf) < 0) return;

    if (buf[byte_idx] & (1 << bit)) {
        buf[byte_idx] &= ~(1 << bit);
        block_write(dev, lba, 1, buf);
        sb->free_blocks++;
        write_super(dev, sb);
    }
}

static uint32_t alloc_inode(block_dev_t* dev, lemon_super_t* sb) {
    if (sb->free_inodes == 0) return 0;

    uint8_t buf[BLOCK_SECTOR_SIZE];
    for (uint32_t s = 0; s < sb->inode_bitmap_sectors; s++) {
        uint32_t lba = sb->inode_bitmap_lba + s;
        if (block_read(dev, lba, 1, buf) < 0) return 0;

        for (uint32_t byte_idx = 0; byte_idx < BLOCK_SECTOR_SIZE; byte_idx++) {
            if (buf[byte_idx] != 0xFF) {
                for (int bit = 0; bit < 8; bit++) {
                    if (!(buf[byte_idx] & (1 << bit))) {
                        uint32_t ino = (s * BLOCK_SECTOR_SIZE + byte_idx) * 8 + bit;
                        if (ino == 0) continue; // Inode 0 is reserved
                        if (ino > sb->total_inodes) return 0;

                        buf[byte_idx] |= (1 << bit);
                        if (block_write(dev, lba, 1, buf) < 0) return 0;

                        sb->free_inodes--;
                        write_super(dev, sb);
                        return ino;
                    }
                }
            }
        }
    }
    return 0;
}

static void free_inode(block_dev_t* dev, lemon_super_t* sb, uint32_t ino) {
    if (ino == 0 || ino > sb->total_inodes) return;

    uint32_t sec_idx = ino / (BLOCK_SECTOR_SIZE * 8);
    uint32_t rem = ino % (BLOCK_SECTOR_SIZE * 8);
    uint32_t byte_idx = rem / 8;
    uint32_t bit = rem % 8;

    uint8_t buf[BLOCK_SECTOR_SIZE];
    uint32_t lba = sb->inode_bitmap_lba + sec_idx;
    if (block_read(dev, lba, 1, buf) < 0) return;

    if (buf[byte_idx] & (1 << bit)) {
        buf[byte_idx] &= ~(1 << bit);
        block_write(dev, lba, 1, buf);
        sb->free_inodes++;
        write_super(dev, sb);
    }
}

// Block Mapping Helper for Inode (Direct & Indirect)
static uint32_t get_inode_block_lba(block_dev_t* dev, lemon_super_t* sb, lemon_inode_t* ino, uint32_t blk_idx, int allocate) {
    if (blk_idx < LEMON_DIRECT_BLOCKS) {
        if (ino->direct_blocks[blk_idx] == 0 && allocate) {
            uint32_t nb = alloc_block(dev, sb);
            if (!nb) return 0;
            ino->direct_blocks[blk_idx] = nb;
            ino->blocks++;
        }
        return ino->direct_blocks[blk_idx];
    }

    uint32_t indirect_idx = blk_idx - LEMON_DIRECT_BLOCKS;
    if (indirect_idx >= 128) return 0; // Max file size reached (10 + 128 blocks = 70KB)

    if (ino->indirect_block == 0) {
        if (!allocate) return 0;
        uint32_t nib = alloc_block(dev, sb);
        if (!nib) return 0;
        ino->indirect_block = nib;
        ino->blocks++;
    }

    uint32_t indirect_table[128];
    if (block_read(dev, ino->indirect_block, 1, indirect_table) < 0) return 0;

    if (indirect_table[indirect_idx] == 0 && allocate) {
        uint32_t nb = alloc_block(dev, sb);
        if (!nb) return 0;
        indirect_table[indirect_idx] = nb;
        ino->blocks++;
        block_write(dev, ino->indirect_block, 1, indirect_table);
    }

    return indirect_table[indirect_idx];
}

// VFS Operations Forward Declarations
static int         lemon_vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, void* buf);
static int         lemon_vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, const void* buf);
static int         lemon_vfs_readdir(vfs_node_t* node, uint32_t index, vfs_dirent_t* out_dirent);
static vfs_node_t* lemon_vfs_finddir(vfs_node_t* node, const char* name);
static vfs_node_t* lemon_vfs_create(vfs_node_t* parent, const char* name, uint16_t mode);
static vfs_node_t* lemon_vfs_mkdir(vfs_node_t* parent, const char* name, uint16_t mode);
static int         lemon_vfs_unlink(vfs_node_t* parent, const char* name);
static int         lemon_vfs_stat(vfs_node_t* node, vfs_stat_t* out_stat);

static void init_vfs_node_from_inode(vfs_node_t* vnode, block_dev_t* dev, uint32_t ino,
                                    const char* name, const lemon_inode_t* inode_data) {
    lemon_strcpy(vnode->name, name, VFS_NAME_MAX);
    vnode->inode = ino;
    vnode->flags = inode_data->type;
    vnode->size = inode_data->size;
    vnode->permissions = inode_data->permissions;
    vnode->uid = inode_data->uid;
    vnode->gid = inode_data->gid;
    vnode->atime = 0;
    vnode->mtime = inode_data->mtime;
    vnode->ctime = inode_data->mtime;

    vnode->read = (inode_data->type == VFS_FILE) ? lemon_vfs_read : NULL;
    vnode->write = (inode_data->type == VFS_FILE) ? lemon_vfs_write : NULL;
    vnode->open = NULL;
    vnode->close = NULL;
    vnode->readdir = (inode_data->type == VFS_DIRECTORY) ? lemon_vfs_readdir : NULL;
    vnode->finddir = (inode_data->type == VFS_DIRECTORY) ? lemon_vfs_finddir : NULL;
    vnode->create = (inode_data->type == VFS_DIRECTORY) ? lemon_vfs_create : NULL;
    vnode->mkdir = (inode_data->type == VFS_DIRECTORY) ? lemon_vfs_mkdir : NULL;
    vnode->unlink = (inode_data->type == VFS_DIRECTORY) ? lemon_vfs_unlink : NULL;
    vnode->stat = lemon_vfs_stat;

    vnode->ptr = dev;
    vnode->mount_point = NULL;
}

// LemonFS VFS Operations Implementations
static int lemon_vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, void* buf) {
    if (!node || !node->ptr || !buf) return -1;
    block_dev_t* dev = (block_dev_t*)node->ptr;

    lemon_super_t sb;
    if (read_super(dev, &sb) < 0) return -2;

    lemon_inode_t inode;
    if (read_inode(dev, &sb, node->inode, &inode) < 0) return -3;

    if (offset >= inode.size) return 0;
    if (offset + size > inode.size) {
        size = inode.size - offset;
    }

    uint8_t sec_buf[BLOCK_SECTOR_SIZE];
    uint8_t* out = (uint8_t*)buf;
    uint32_t bytes_read = 0;

    while (bytes_read < size) {
        uint32_t cur_offset = offset + bytes_read;
        uint32_t blk_idx = cur_offset / BLOCK_SECTOR_SIZE;
        uint32_t sec_off = cur_offset % BLOCK_SECTOR_SIZE;
        uint32_t to_copy = BLOCK_SECTOR_SIZE - sec_off;
        if (to_copy > (size - bytes_read)) {
            to_copy = size - bytes_read;
        }

        uint32_t lba = get_inode_block_lba(dev, &sb, &inode, blk_idx, 0);
        if (lba == 0) {
            for (uint32_t i = 0; i < to_copy; i++) {
                out[bytes_read + i] = 0;
            }
        } else {
            if (block_read(dev, lba, 1, sec_buf) < 0) break;
            for (uint32_t i = 0; i < to_copy; i++) {
                out[bytes_read + i] = sec_buf[sec_off + i];
            }
        }

        bytes_read += to_copy;
    }

    return bytes_read;
}

static int lemon_vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, const void* buf) {
    if (!node || !node->ptr || !buf) return -1;
    block_dev_t* dev = (block_dev_t*)node->ptr;

    lemon_super_t sb;
    if (read_super(dev, &sb) < 0) return -2;

    lemon_inode_t inode;
    if (read_inode(dev, &sb, node->inode, &inode) < 0) return -3;

    uint8_t sec_buf[BLOCK_SECTOR_SIZE];
    const uint8_t* in = (const uint8_t*)buf;
    uint32_t bytes_written = 0;

    while (bytes_written < size) {
        uint32_t cur_offset = offset + bytes_written;
        uint32_t blk_idx = cur_offset / BLOCK_SECTOR_SIZE;
        uint32_t sec_off = cur_offset % BLOCK_SECTOR_SIZE;
        uint32_t to_copy = BLOCK_SECTOR_SIZE - sec_off;
        if (to_copy > (size - bytes_written)) {
            to_copy = size - bytes_written;
        }

        uint32_t lba = get_inode_block_lba(dev, &sb, &inode, blk_idx, 1);
        if (lba == 0) break; // Out of disk space

        if (sec_off == 0 && to_copy == BLOCK_SECTOR_SIZE) {
            if (block_write(dev, lba, 1, in + bytes_written) < 0) break;
        } else {
            if (block_read(dev, lba, 1, sec_buf) < 0) break;
            for (uint32_t i = 0; i < to_copy; i++) {
                sec_buf[sec_off + i] = in[bytes_written + i];
            }
            if (block_write(dev, lba, 1, sec_buf) < 0) break;
        }

        bytes_written += to_copy;
    }

    if (offset + bytes_written > inode.size) {
        inode.size = offset + bytes_written;
    }

    write_inode(dev, &sb, node->inode, &inode);
    node->size = inode.size;

    return bytes_written;
}

static int lemon_vfs_readdir(vfs_node_t* node, uint32_t index, vfs_dirent_t* out_dirent) {
    if (!node || !node->ptr || !out_dirent) return -1;
    block_dev_t* dev = (block_dev_t*)node->ptr;

    lemon_super_t sb;
    if (read_super(dev, &sb) < 0) return -2;

    lemon_inode_t dir_ino;
    if (read_inode(dev, &sb, node->inode, &dir_ino) < 0) return -3;

    lemon_dirent_t dirents[LEMON_DIRENTS_PER_BLOCK];
    uint32_t match_count = 0;

    for (uint32_t b = 0; b < LEMON_DIRECT_BLOCKS; b++) {
        uint32_t lba = dir_ino.direct_blocks[b];
        if (lba == 0) continue;

        if (block_read(dev, lba, 1, dirents) < 0) break;

        for (int i = 0; i < LEMON_DIRENTS_PER_BLOCK; i++) {
            if (dirents[i].inode != 0) {
                if (match_count == index) {
                    lemon_strcpy(out_dirent->name, dirents[i].name, VFS_NAME_MAX);
                    out_dirent->inode = dirents[i].inode;
                    out_dirent->type = dirents[i].type;
                    return 0;
                }
                match_count++;
            }
        }
    }

    return -1; // End of directory
}

static vfs_node_t* lemon_vfs_finddir(vfs_node_t* node, const char* name) {
    if (!node || !node->ptr || !name) return NULL;
    block_dev_t* dev = (block_dev_t*)node->ptr;

    lemon_super_t sb;
    if (read_super(dev, &sb) < 0) return NULL;

    lemon_inode_t dir_ino;
    if (read_inode(dev, &sb, node->inode, &dir_ino) < 0) return NULL;

    lemon_dirent_t dirents[LEMON_DIRENTS_PER_BLOCK];

    for (uint32_t b = 0; b < LEMON_DIRECT_BLOCKS; b++) {
        uint32_t lba = dir_ino.direct_blocks[b];
        if (lba == 0) continue;

        if (block_read(dev, lba, 1, dirents) < 0) break;

        for (int i = 0; i < LEMON_DIRENTS_PER_BLOCK; i++) {
            if (dirents[i].inode != 0 && lemon_strcmp(dirents[i].name, name) == 0) {
                lemon_inode_t target_ino;
                if (read_inode(dev, &sb, dirents[i].inode, &target_ino) < 0) return NULL;

                vfs_node_t* child = alloc_vfs_node();
                init_vfs_node_from_inode(child, dev, dirents[i].inode, dirents[i].name, &target_ino);
                return child;
            }
        }
    }

    return NULL;
}

static vfs_node_t* lemon_vfs_create(vfs_node_t* parent, const char* name, uint16_t mode) {
    if (!parent || !parent->ptr || !name) return NULL;
    block_dev_t* dev = (block_dev_t*)parent->ptr;

    lemon_super_t sb;
    if (read_super(dev, &sb) < 0) return NULL;

    // Check if entry already exists
    if (lemon_vfs_finddir(parent, name) != NULL) return NULL;

    // Allocate new inode
    uint32_t new_ino = alloc_inode(dev, &sb);
    if (new_ino == 0) return NULL;

    lemon_inode_t child_inode;
    child_inode.type = VFS_FILE;
    child_inode.permissions = mode ? mode : 0644;
    child_inode.size = 0;
    child_inode.blocks = 0;
    for (int i = 0; i < LEMON_DIRECT_BLOCKS; i++) child_inode.direct_blocks[i] = 0;
    child_inode.indirect_block = 0;
    child_inode.uid = 0;
    child_inode.gid = 0;
    child_inode.mtime = 0;

    if (write_inode(dev, &sb, new_ino, &child_inode) < 0) {
        free_inode(dev, &sb, new_ino);
        return NULL;
    }

    // Insert entry into parent directory
    lemon_inode_t parent_ino;
    if (read_inode(dev, &sb, parent->inode, &parent_ino) < 0) {
        free_inode(dev, &sb, new_ino);
        return NULL;
    }

    lemon_dirent_t dirents[LEMON_DIRENTS_PER_BLOCK];
    int inserted = 0;

    for (uint32_t b = 0; b < LEMON_DIRECT_BLOCKS; b++) {
        uint32_t lba = parent_ino.direct_blocks[b];
        if (lba == 0) {
            // Allocate new block for parent dir
            lba = alloc_block(dev, &sb);
            if (!lba) break;
            parent_ino.direct_blocks[b] = lba;
            parent_ino.blocks++;
            write_inode(dev, &sb, parent->inode, &parent_ino);
        }

        if (block_read(dev, lba, 1, dirents) < 0) break;

        for (int i = 0; i < LEMON_DIRENTS_PER_BLOCK; i++) {
            if (dirents[i].inode == 0) {
                dirents[i].inode = new_ino;
                dirents[i].type = VFS_FILE;
                lemon_strcpy(dirents[i].name, name, LEMON_NAME_MAX);
                block_write(dev, lba, 1, dirents);
                inserted = 1;
                break;
            }
        }
        if (inserted) break;
    }

    if (!inserted) {
        free_inode(dev, &sb, new_ino);
        return NULL;
    }

    vfs_node_t* child_node = alloc_vfs_node();
    init_vfs_node_from_inode(child_node, dev, new_ino, name, &child_inode);
    return child_node;
}

static vfs_node_t* lemon_vfs_mkdir(vfs_node_t* parent, const char* name, uint16_t mode) {
    if (!parent || !parent->ptr || !name) return NULL;
    block_dev_t* dev = (block_dev_t*)parent->ptr;

    lemon_super_t sb;
    if (read_super(dev, &sb) < 0) return NULL;

    if (lemon_vfs_finddir(parent, name) != NULL) return NULL;

    uint32_t new_ino = alloc_inode(dev, &sb);
    if (new_ino == 0) return NULL;

    uint32_t dir_blk = alloc_block(dev, &sb);
    if (dir_blk == 0) {
        free_inode(dev, &sb, new_ino);
        return NULL;
    }

    // Initialize . and .. entries in new directory
    lemon_dirent_t dirents[LEMON_DIRENTS_PER_BLOCK];
    for (int i = 0; i < LEMON_DIRENTS_PER_BLOCK; i++) dirents[i].inode = 0;

    dirents[0].inode = new_ino;
    dirents[0].type = VFS_DIRECTORY;
    lemon_strcpy(dirents[0].name, ".", LEMON_NAME_MAX);

    dirents[1].inode = parent->inode;
    dirents[1].type = VFS_DIRECTORY;
    lemon_strcpy(dirents[1].name, "..", LEMON_NAME_MAX);

    block_write(dev, dir_blk, 1, dirents);

    lemon_inode_t child_inode;
    child_inode.type = VFS_DIRECTORY;
    child_inode.permissions = mode ? mode : 0755;
    child_inode.size = 2 * sizeof(lemon_dirent_t);
    child_inode.blocks = 1;
    child_inode.direct_blocks[0] = dir_blk;
    for (int i = 1; i < LEMON_DIRECT_BLOCKS; i++) child_inode.direct_blocks[i] = 0;
    child_inode.indirect_block = 0;
    child_inode.uid = 0;
    child_inode.gid = 0;
    child_inode.mtime = 0;

    write_inode(dev, &sb, new_ino, &child_inode);

    // Insert entry into parent directory
    lemon_inode_t parent_ino;
    read_inode(dev, &sb, parent->inode, &parent_ino);

    int inserted = 0;
    for (uint32_t b = 0; b < LEMON_DIRECT_BLOCKS; b++) {
        uint32_t lba = parent_ino.direct_blocks[b];
        if (lba == 0) {
            lba = alloc_block(dev, &sb);
            if (!lba) break;
            parent_ino.direct_blocks[b] = lba;
            parent_ino.blocks++;
            write_inode(dev, &sb, parent->inode, &parent_ino);
        }

        if (block_read(dev, lba, 1, dirents) < 0) break;

        for (int i = 0; i < LEMON_DIRENTS_PER_BLOCK; i++) {
            if (dirents[i].inode == 0) {
                dirents[i].inode = new_ino;
                dirents[i].type = VFS_DIRECTORY;
                lemon_strcpy(dirents[i].name, name, LEMON_NAME_MAX);
                block_write(dev, lba, 1, dirents);
                inserted = 1;
                break;
            }
        }
        if (inserted) break;
    }

    if (!inserted) {
        free_block(dev, &sb, dir_blk);
        free_inode(dev, &sb, new_ino);
        return NULL;
    }

    vfs_node_t* child_node = alloc_vfs_node();
    init_vfs_node_from_inode(child_node, dev, new_ino, name, &child_inode);
    return child_node;
}

static int lemon_vfs_unlink(vfs_node_t* parent, const char* name) {
    if (!parent || !parent->ptr || !name) return -1;
    block_dev_t* dev = (block_dev_t*)parent->ptr;

    lemon_super_t sb;
    if (read_super(dev, &sb) < 0) return -2;

    lemon_inode_t parent_ino;
    if (read_inode(dev, &sb, parent->inode, &parent_ino) < 0) return -3;

    lemon_dirent_t dirents[LEMON_DIRENTS_PER_BLOCK];
    uint32_t target_ino_num = 0;
    uint32_t target_lba = 0;
    int target_idx = -1;

    for (uint32_t b = 0; b < LEMON_DIRECT_BLOCKS; b++) {
        uint32_t lba = parent_ino.direct_blocks[b];
        if (lba == 0) continue;

        if (block_read(dev, lba, 1, dirents) < 0) break;

        for (int i = 0; i < LEMON_DIRENTS_PER_BLOCK; i++) {
            if (dirents[i].inode != 0 && lemon_strcmp(dirents[i].name, name) == 0) {
                target_ino_num = dirents[i].inode;
                target_lba = lba;
                target_idx = i;
                break;
            }
        }
        if (target_ino_num != 0) break;
    }

    if (target_ino_num == 0) return -4; // Not found

    // Read target inode and release its blocks
    lemon_inode_t target_ino;
    if (read_inode(dev, &sb, target_ino_num, &target_ino) == 0) {
        for (uint32_t b = 0; b < LEMON_DIRECT_BLOCKS; b++) {
            if (target_ino.direct_blocks[b] != 0) {
                free_block(dev, &sb, target_ino.direct_blocks[b]);
            }
        }
        if (target_ino.indirect_block != 0) {
            uint32_t indirect_table[128];
            if (block_read(dev, target_ino.indirect_block, 1, indirect_table) == 0) {
                for (int i = 0; i < 128; i++) {
                    if (indirect_table[i] != 0) {
                        free_block(dev, &sb, indirect_table[i]);
                    }
                }
            }
            free_block(dev, &sb, target_ino.indirect_block);
        }
    }

    // Free the inode
    free_inode(dev, &sb, target_ino_num);

    // Remove directory entry from parent
    if (block_read(dev, target_lba, 1, dirents) == 0) {
        dirents[target_idx].inode = 0;
        dirents[target_idx].name[0] = '\0';
        block_write(dev, target_lba, 1, dirents);
    }

    return 0;
}

static int lemon_vfs_stat(vfs_node_t* node, vfs_stat_t* out_stat) {
    if (!node || !out_stat) return -1;
    out_stat->inode = node->inode;
    out_stat->size = node->size;
    out_stat->flags = node->flags;
    out_stat->permissions = node->permissions;
    out_stat->uid = node->uid;
    out_stat->gid = node->gid;
    out_stat->atime = node->atime;
    out_stat->mtime = node->mtime;
    out_stat->ctime = node->ctime;
    return 0;
}

// Formatting & Mounting API
int fs_lemon_format(block_dev_t* dev) {
    if (!dev || (dev->flags & BLOCK_FLAG_READONLY)) return -1;

    serial_print("[LemonFS] Formatting block device '");
    serial_print(dev->name);
    serial_println("' with LemonFS filesystem...");

    lemon_super_t sb;
    sb.magic = LEMON_FS_MAGIC;
    sb.version = LEMON_FS_VERSION;
    sb.block_size = BLOCK_SECTOR_SIZE;
    sb.total_blocks = dev->total_sectors;
    sb.total_inodes = LEMON_TOTAL_INODES;

    // Layout:
    // Sector 0: MBR / Reserved
    // Sector 1: Superblock
    // Sector 2: Block bitmap (1 sector tracks 4096 blocks = 2MB. Cap to 1 for simple ramdisk/disk)
    sb.block_bitmap_lba = 2;
    sb.block_bitmap_sectors = 1;
    if (sb.total_blocks > 4096) {
        sb.block_bitmap_sectors = (sb.total_blocks + 4095) / 4096;
        if (sb.block_bitmap_sectors > 16) sb.block_bitmap_sectors = 16;
    }

    sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sectors;
    sb.inode_bitmap_sectors = 1;

    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sectors;
    sb.inode_table_sectors = LEMON_TOTAL_INODES / LEMON_INODES_PER_BLOCK; // 256 / 8 = 32 sectors

    sb.data_blocks_lba = sb.inode_table_lba + sb.inode_table_sectors;
    sb.free_blocks = sb.total_blocks - sb.data_blocks_lba;
    sb.free_inodes = sb.total_inodes - 1; // Inode 1 is root dir
    sb.root_inode = 1;

    for (int p = 0; p < 452; p++) sb.padding[p] = 0;

    // Write zeroed block bitmap
    uint8_t zero_buf[BLOCK_SECTOR_SIZE];
    for (int i = 0; i < BLOCK_SECTOR_SIZE; i++) zero_buf[i] = 0;

    for (uint32_t s = 0; s < sb.block_bitmap_sectors; s++) {
        block_write(dev, sb.block_bitmap_lba + s, 1, zero_buf);
    }

    // Mark system blocks (0 through data_blocks_lba) as allocated in block bitmap
    for (uint32_t b = 0; b <= sb.data_blocks_lba; b++) {
        uint32_t sec_idx = b / (BLOCK_SECTOR_SIZE * 8);
        uint32_t rem = b % (BLOCK_SECTOR_SIZE * 8);
        uint32_t byte_idx = rem / 8;
        uint32_t bit = rem % 8;

        uint8_t bmap_buf[BLOCK_SECTOR_SIZE];
        block_read(dev, sb.block_bitmap_lba + sec_idx, 1, bmap_buf);
        bmap_buf[byte_idx] |= (1 << bit);
        block_write(dev, sb.block_bitmap_lba + sec_idx, 1, bmap_buf);
    }

    // Write zeroed inode bitmap
    block_write(dev, sb.inode_bitmap_lba, 1, zero_buf);

    // Mark Inode 1 (Root Dir) as allocated in inode bitmap
    uint8_t imap_buf[BLOCK_SECTOR_SIZE];
    block_read(dev, sb.inode_bitmap_lba, 1, imap_buf);
    imap_buf[0] |= 0x03; // Inode 0 (reserved) and Inode 1 (root) marked used
    block_write(dev, sb.inode_bitmap_lba, 1, imap_buf);

    // Initialize Inode Table sectors
    for (uint32_t s = 0; s < sb.inode_table_sectors; s++) {
        block_write(dev, sb.inode_table_lba + s, 1, zero_buf);
    }

    // Write Root Directory Inode (Inode 1)
    lemon_inode_t root_ino;
    root_ino.type = VFS_DIRECTORY;
    root_ino.permissions = 0755;
    root_ino.size = 2 * sizeof(lemon_dirent_t);
    root_ino.blocks = 1;
    root_ino.direct_blocks[0] = sb.data_blocks_lba; // Root directory takes first data block
    for (int d = 1; d < LEMON_DIRECT_BLOCKS; d++) root_ino.direct_blocks[d] = 0;
    root_ino.indirect_block = 0;
    root_ino.uid = 0;
    root_ino.gid = 0;
    root_ino.mtime = 0;

    write_inode(dev, &sb, 1, &root_ino);

    // Populate Root Directory data block with '.' and '..'
    lemon_dirent_t root_dirents[LEMON_DIRENTS_PER_BLOCK];
    for (int i = 0; i < LEMON_DIRENTS_PER_BLOCK; i++) root_dirents[i].inode = 0;

    root_dirents[0].inode = 1;
    root_dirents[0].type = VFS_DIRECTORY;
    lemon_strcpy(root_dirents[0].name, ".", LEMON_NAME_MAX);

    root_dirents[1].inode = 1;
    root_dirents[1].type = VFS_DIRECTORY;
    lemon_strcpy(root_dirents[1].name, "..", LEMON_NAME_MAX);

    block_write(dev, sb.data_blocks_lba, 1, root_dirents);

    // Write Superblock
    write_super(dev, &sb);

    serial_println("[LemonFS] Formatting completed successfully.");
    return 0;
}

vfs_node_t* fs_lemon_mount(block_dev_t* dev) {
    if (!dev) return NULL;

    lemon_super_t sb;
    if (read_super(dev, &sb) < 0) {
        serial_println("[LemonFS] No valid LemonFS superblock found, formatting...");
        if (fs_lemon_format(dev) < 0) {
            serial_println("[LemonFS] Format failed!");
            return NULL;
        }
        if (read_super(dev, &sb) < 0) {
            serial_println("[LemonFS] Mount failed after format!");
            return NULL;
        }
    }

    lemon_inode_t root_ino;
    if (read_inode(dev, &sb, 1, &root_ino) < 0) {
        serial_println("[LemonFS] Could not read root inode!");
        return NULL;
    }

    vfs_node_t* root_vnode = &node_pool[0];
    node_pool_used[0] = 1;
    init_vfs_node_from_inode(root_vnode, dev, 1, "/", &root_ino);

    serial_print("[LemonFS] Mounted filesystem on device '");
    serial_print(dev->name);
    serial_println("' as root VFS node.");

    return root_vnode;
}

// LemonFS Automated Verification Suite
void fs_lemon_run_tests(block_dev_t* dev) {
    serial_println("=========================================");
    serial_println("[LemonFS] Running Block Filesystem Verification...");
    serial_println("=========================================");

    if (!dev) {
        serial_println("  -> [TEST 1/5] Device validity: FAIL");
        return;
    }
    serial_println("  -> [TEST 1/5] Device validity: PASS");

    // Format & Mount
    if (fs_lemon_format(dev) < 0) {
        serial_println("  -> [TEST 2/5] fs_lemon_format: FAIL");
        return;
    }
    serial_println("  -> [TEST 2/5] fs_lemon_format: PASS");

    vfs_node_t* root = fs_lemon_mount(dev);
    if (!root || !root->create || !root->mkdir) {
        serial_println("  -> [TEST 3/5] fs_lemon_mount: FAIL");
        return;
    }
    serial_println("  -> [TEST 3/5] fs_lemon_mount: PASS");

    // Test Multi-block File Write & Read
    vfs_node_t* test_file = root->create(root, "storage_test.dat", 0644);
    if (!test_file || !test_file->write || !test_file->read) {
        serial_println("  -> [TEST 4/5] Multi-block file create: FAIL");
        return;
    }

    char write_buf[1024];
    for (int i = 0; i < 1024; i++) {
        write_buf[i] = (char)('A' + (i % 26));
    }

    int written = test_file->write(test_file, 0, 1024, write_buf);
    char read_buf[1024];
    for (int i = 0; i < 1024; i++) read_buf[i] = 0;

    int read_bytes = test_file->read(test_file, 0, 1024, read_buf);

    int match = 1;
    for (int i = 0; i < 1024; i++) {
        if (read_buf[i] != write_buf[i]) {
            match = 0;
            break;
        }
    }

    if (written == 1024 && read_bytes == 1024 && match) {
        serial_println("  -> [TEST 4/5] Multi-block (1024-byte) read/write verification: PASS");
    } else {
        serial_println("  -> [TEST 4/5] Multi-block read/write: FAIL");
        return;
    }

    // Directory creation and entry lookup
    vfs_node_t* test_dir = root->mkdir(root, "system_test", 0755);
    if (test_dir && root->finddir(root, "system_test") != NULL) {
        serial_println("  -> [TEST 5/5] Inode directory creation & finddir: PASS");
    } else {
        serial_println("  -> [TEST 5/5] Inode directory creation: FAIL");
        return;
    }

    serial_println("[LemonFS] Block Filesystem Driver: 5/5 PASSED!");
}
