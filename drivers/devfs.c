#include "devfs.h"
#include "entropy.h"
#include "block.h"
#include "serial.h"
#include "vga.h"
#include <stdint.h>
#include <stddef.h>

#define DEVFS_MAX_DEVICES 16

static vfs_node_t devfs_root;
static vfs_node_t devfs_nodes[DEVFS_MAX_DEVICES];
static int        devfs_count = 0;
static uint32_t   urandom_seed = 0x19870422;

static int devfs_strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static void devfs_strcpy(char* d, const char* s, int max) {
    int i = 0;
    while (s && s[i] && i < max - 1) { d[i] = s[i]; i++; }
    d[i] = '\0';
}

// /dev/null
static int null_read(vfs_node_t* node, uint32_t offset, uint32_t size, void* buf) {
    (void)node; (void)offset; (void)size; (void)buf;
    return 0; // EOF
}

static int null_write(vfs_node_t* node, uint32_t offset, uint32_t size, const void* buf) {
    (void)node; (void)offset; (void)buf;
    return size; // Discard data and succeed
}

// /dev/zero
static int zero_read(vfs_node_t* node, uint32_t offset, uint32_t size, void* buf) {
    (void)node; (void)offset;
    if (!buf) return 0;
    uint8_t* p = (uint8_t*)buf;
    for (uint32_t i = 0; i < size; i++) {
        p[i] = 0;
    }
    return size;
}

static int zero_write(vfs_node_t* node, uint32_t offset, uint32_t size, const void* buf) {
    (void)node; (void)offset; (void)buf;
    return size;
}

// /dev/urandom
static uint32_t lcg_rand(void) {
    urandom_seed = (urandom_seed * 1103515245 + 12345) & 0x7FFFFFFF;
    return urandom_seed;
}

static int urandom_read(vfs_node_t* node, uint32_t offset, uint32_t size, void* buf) {
    (void)node; (void)offset;
    if (!buf || size == 0) return 0;
    entropy_get_bytes((uint8_t*)buf, size);
    return size;
}

static int urandom_write(vfs_node_t* node, uint32_t offset, uint32_t size, const void* buf) {
    (void)node; (void)offset;
    if (!buf) return 0;
    const uint8_t* p = (const uint8_t*)buf;
    for (uint32_t i = 0; i < size; i++) {
        urandom_seed = (urandom_seed << 5) ^ p[i];
    }
    return size;
}

#include "keyboard.h"

// /dev/tty
static int tty_read(vfs_node_t* node, uint32_t offset, uint32_t size, void* buf) {
    (void)node; (void)offset;
    if (!buf || size == 0) return 0;
    char* p = (char*)buf;
    uint32_t count = 0;
    while (count < size) {
        poll_keyboard();
        if (has_key()) {
            char c = getchar();
            p[count++] = c;
            if (c == '\n' || c == '\r') break;
        } else if (serial_has_char()) {
            char c = serial_getchar();
            if (c == '\r') c = '\n';
            p[count++] = c;
            if (c == '\n') break;
        } else {
            asm volatile("pause");
        }
    }
    return count;
}

static int tty_write(vfs_node_t* node, uint32_t offset, uint32_t size, const void* buf) {
    (void)node; (void)offset;
    if (!buf) return 0;
    const char* c = (const char*)buf;
    for (uint32_t i = 0; i < size; i++) {
        vga_putchar(c[i]);
        serial_putchar(c[i]);
    }
    extern void gui_terminal_write(const char* text, size_t count);
    gui_terminal_write(c, size);
    return size;
}

// Generic Block Device Adapter (/dev/hda, /dev/ramdisk0, etc.)
static int blkdev_read(vfs_node_t* node, uint32_t offset, uint32_t size, void* buf) {
    if (!node || !node->ptr || !buf) return -1;
    block_dev_t* dev = (block_dev_t*)node->ptr;

    uint32_t dev_capacity_bytes = dev->total_sectors * dev->sector_size;
    if (offset >= dev_capacity_bytes) return 0;
    if (offset + size > dev_capacity_bytes) {
        size = dev_capacity_bytes - offset;
    }

    uint8_t sector_buf[BLOCK_SECTOR_SIZE];
    uint8_t* out = (uint8_t*)buf;
    uint32_t bytes_transferred = 0;

    while (bytes_transferred < size) {
        uint32_t cur_offset = offset + bytes_transferred;
        uint32_t lba = cur_offset / dev->sector_size;
        uint32_t sec_off = cur_offset % dev->sector_size;
        uint32_t to_copy = dev->sector_size - sec_off;
        if (to_copy > (size - bytes_transferred)) {
            to_copy = size - bytes_transferred;
        }

        if (sec_off == 0 && to_copy == dev->sector_size) {
            // Direct sector read
            if (block_read(dev, lba, 1, out + bytes_transferred) < 0) {
                break;
            }
        } else {
            // Partial sector read
            if (block_read(dev, lba, 1, sector_buf) < 0) {
                break;
            }
            for (uint32_t i = 0; i < to_copy; i++) {
                out[bytes_transferred + i] = sector_buf[sec_off + i];
            }
        }

        bytes_transferred += to_copy;
    }

    return bytes_transferred;
}

static int blkdev_write(vfs_node_t* node, uint32_t offset, uint32_t size, const void* buf) {
    if (!node || !node->ptr || !buf) return -1;
    block_dev_t* dev = (block_dev_t*)node->ptr;

    if (dev->flags & BLOCK_FLAG_READONLY) return -2;

    uint32_t dev_capacity_bytes = dev->total_sectors * dev->sector_size;
    if (offset >= dev_capacity_bytes) return 0;
    if (offset + size > dev_capacity_bytes) {
        size = dev_capacity_bytes - offset;
    }

    uint8_t sector_buf[BLOCK_SECTOR_SIZE];
    const uint8_t* in = (const uint8_t*)buf;
    uint32_t bytes_transferred = 0;

    while (bytes_transferred < size) {
        uint32_t cur_offset = offset + bytes_transferred;
        uint32_t lba = cur_offset / dev->sector_size;
        uint32_t sec_off = cur_offset % dev->sector_size;
        uint32_t to_copy = dev->sector_size - sec_off;
        if (to_copy > (size - bytes_transferred)) {
            to_copy = size - bytes_transferred;
        }

        if (sec_off == 0 && to_copy == dev->sector_size) {
            // Full aligned sector
            if (block_write(dev, lba, 1, in + bytes_transferred) < 0) {
                break;
            }
        } else {
            // Read-Modify-Write
            if (block_read(dev, lba, 1, sector_buf) < 0) {
                break;
            }
            for (uint32_t i = 0; i < to_copy; i++) {
                sector_buf[sec_off + i] = in[bytes_transferred + i];
            }
            if (block_write(dev, lba, 1, sector_buf) < 0) {
                break;
            }
        }

        bytes_transferred += to_copy;
    }

    return bytes_transferred;
}

// DevFS Directory Operations
static int devfs_readdir(vfs_node_t* node, uint32_t index, vfs_dirent_t* out_dirent) {
    (void)node;
    if (!out_dirent || (int)index >= devfs_count) return -1;

    devfs_strcpy(out_dirent->name, devfs_nodes[index].name, VFS_NAME_MAX);
    out_dirent->inode = devfs_nodes[index].inode;
    out_dirent->type = devfs_nodes[index].flags;
    return 0;
}

static vfs_node_t* devfs_finddir(vfs_node_t* node, const char* name) {
    (void)node;
    if (!name) return NULL;

    for (int i = 0; i < devfs_count; i++) {
        if (devfs_strcmp(devfs_nodes[i].name, name) == 0) {
            return &devfs_nodes[i];
        }
    }
    return NULL;
}

static int devfs_stat(vfs_node_t* node, vfs_stat_t* out_stat) {
    if (!node || !out_stat) return -1;
    out_stat->inode = node->inode;
    out_stat->size = node->size;
    out_stat->flags = node->flags;
    out_stat->permissions = node->permissions;
    return 0;
}

// Register a Device Node in DevFS
static void devfs_register_node(const char* name, uint32_t flags, uint32_t size,
                                vfs_read_fn r_fn, vfs_write_fn w_fn, void* priv) {
    if (devfs_count >= DEVFS_MAX_DEVICES) return;

    vfs_node_t* n = &devfs_nodes[devfs_count];
    devfs_strcpy(n->name, name, VFS_NAME_MAX);
    n->flags = flags;
    n->inode = devfs_count + 100;
    n->size = size;
    n->permissions = 0666;
    n->uid = 0;
    n->gid = 0;
    n->atime = 0;
    n->mtime = 0;
    n->ctime = 0;

    n->read = r_fn;
    n->write = w_fn;
    n->open = NULL;
    n->close = NULL;
    n->readdir = NULL;
    n->finddir = NULL;
    n->create = NULL;
    n->mkdir = NULL;
    n->unlink = NULL;
    n->stat = devfs_stat;
    n->ptr = priv;
    n->mount_point = NULL;

    devfs_count++;
}

void devfs_init(void) {
    serial_println("[DevFS] Initializing Device File System...");

    // Setup Root Directory Node
    devfs_strcpy(devfs_root.name, "dev", VFS_NAME_MAX);
    devfs_root.flags = VFS_DIRECTORY;
    devfs_root.inode = 1;
    devfs_root.size = 0;
    devfs_root.permissions = 0755;
    devfs_root.uid = 0;
    devfs_root.gid = 0;
    devfs_root.read = NULL;
    devfs_root.write = NULL;
    devfs_root.open = NULL;
    devfs_root.close = NULL;
    devfs_root.readdir = devfs_readdir;
    devfs_root.finddir = devfs_finddir;
    devfs_root.create = NULL;
    devfs_root.mkdir = NULL;
    devfs_root.unlink = NULL;
    devfs_root.stat = devfs_stat;
    devfs_root.ptr = NULL;
    devfs_root.mount_point = NULL;

    devfs_count = 0;

    // Character Devices
    devfs_register_node("null", VFS_CHARDEVICE, 0, null_read, null_write, NULL);
    devfs_register_node("zero", VFS_CHARDEVICE, 0, zero_read, zero_write, NULL);
    devfs_register_node("urandom", VFS_CHARDEVICE, 0, urandom_read, urandom_write, NULL);
    devfs_register_node("random", VFS_CHARDEVICE, 0, urandom_read, urandom_write, NULL);
    devfs_register_node("tty", VFS_CHARDEVICE, 0, tty_read, tty_write, NULL);
    devfs_register_node("console", VFS_CHARDEVICE, 0, tty_read, tty_write, NULL);

    // Block Devices (if registered in block layer)
    block_dev_t* hda = block_get_device("hda");
    if (hda) {
        devfs_register_node("hda", VFS_BLOCKDEVICE, hda->total_sectors * hda->sector_size,
                            blkdev_read, blkdev_write, hda);
        serial_println("  -> Registered /dev/hda (ATA Primary Master)");
    }

    block_dev_t* ramdisk0 = block_get_device("ramdisk0");
    if (ramdisk0) {
        devfs_register_node("ramdisk0", VFS_BLOCKDEVICE, ramdisk0->total_sectors * ramdisk0->sector_size,
                            blkdev_read, blkdev_write, ramdisk0);
        serial_println("  -> Registered /dev/ramdisk0 (RAM Storage)");
    }

    // Mount DevFS at /dev
    vfs_mount("/dev", &devfs_root);
    serial_println("[DevFS] DevFS mounted at /dev successfully.");
}

vfs_node_t* devfs_get_root(void) {
    return &devfs_root;
}
