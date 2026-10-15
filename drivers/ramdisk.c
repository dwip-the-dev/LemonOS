#include "ramdisk.h"
#include "block.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

static uint8_t ramdisk_storage[RAMDISK_SECTORS * BLOCK_SECTOR_SIZE];

static int ramdisk_read_impl(block_dev_t* dev, uint32_t lba, uint32_t count, void* buf) {
    (void)dev;
    if (lba + count > RAMDISK_SECTORS) return -1;
    uint32_t offset = lba * BLOCK_SECTOR_SIZE;
    uint32_t bytes = count * BLOCK_SECTOR_SIZE;

    const uint8_t* src = &ramdisk_storage[offset];
    uint8_t* dst = (uint8_t*)buf;
    for (uint32_t i = 0; i < bytes; i++) {
        dst[i] = src[i];
    }
    return 0;
}

static int ramdisk_write_impl(block_dev_t* dev, uint32_t lba, uint32_t count, const void* buf) {
    (void)dev;
    if (lba + count > RAMDISK_SECTORS) return -1;
    uint32_t offset = lba * BLOCK_SECTOR_SIZE;
    uint32_t bytes = count * BLOCK_SECTOR_SIZE;

    const uint8_t* src = (const uint8_t*)buf;
    uint8_t* dst = &ramdisk_storage[offset];
    for (uint32_t i = 0; i < bytes; i++) {
        dst[i] = src[i];
    }
    return 0;
}

static int ramdisk_flush_impl(block_dev_t* dev) {
    (void)dev;
    return 0;
}

static block_dev_t ramdisk_dev = {
    .name = "ramdisk0",
    .sector_size = BLOCK_SECTOR_SIZE,
    .total_sectors = RAMDISK_SECTORS,
    .flags = BLOCK_FLAG_RAMDISK,
    .priv_data = ramdisk_storage,
    .read_sectors = ramdisk_read_impl,
    .write_sectors = ramdisk_write_impl,
    .flush = ramdisk_flush_impl
};

void ramdisk_init(void) {
    serial_println("[RAMDISK] Initializing 2MB RAM-backed block storage device...");

    // Zero out memory
    for (uint32_t i = 0; i < sizeof(ramdisk_storage); i++) {
        ramdisk_storage[i] = 0;
    }

    block_register_device(&ramdisk_dev);
}
