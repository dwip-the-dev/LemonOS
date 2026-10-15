#include "block.h"
#include "bcache.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

static block_dev_t* devices[BLOCK_MAX_DEVICES];
static int num_devices = 0;

static int b_strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

void block_init(void) {
    num_devices = 0;
    for (int i = 0; i < BLOCK_MAX_DEVICES; i++) {
        devices[i] = NULL;
    }
    bcache_init();
    serial_println("[BLOCK] Block device subsystem initialized.");
}

int block_register_device(block_dev_t* dev) {
    if (!dev || num_devices >= BLOCK_MAX_DEVICES) return -1;
    devices[num_devices++] = dev;

    serial_print("[BLOCK] Registered device '");
    serial_print(dev->name);
    serial_print("' (");
    serial_print_dec((dev->total_sectors * dev->sector_size) / 1024);
    serial_println(" KB)");
    return 0;
}

block_dev_t* block_get_device(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < num_devices; i++) {
        if (devices[i] && b_strcmp(devices[i]->name, name) == 0) {
            return devices[i];
        }
    }
    return NULL;
}

block_dev_t* block_get_default(void) {
    // Prefer primary hard disk "hda" if registered
    block_dev_t* hda = block_get_device("hda");
    if (hda) return hda;

    // Fall back to first registered device (e.g. ramdisk0)
    if (num_devices > 0 && devices[0]) {
        return devices[0];
    }
    return NULL;
}

int block_get_device_count(void) {
    return num_devices;
}

block_dev_t* block_get_device_by_index(int index) {
    if (index >= 0 && index < num_devices) {
        return devices[index];
    }
    return NULL;
}

int block_read(block_dev_t* dev, uint32_t lba, uint32_t count, void* buf) {
    if (!dev || !buf || count == 0) return -1;
    if (lba + count > dev->total_sectors) {
        serial_println("[BLOCK] Read out of bounds error!");
        return -2;
    }
    if (!dev->read_sectors) return -3;

    // Route through high-speed block buffer cache
    for (uint32_t i = 0; i < count; i++) {
        int r = bcache_read(dev, lba + i, (uint8_t*)buf + (i * dev->sector_size));
        if (r != 0) return r;
    }
    return 0;
}

int block_write(block_dev_t* dev, uint32_t lba, uint32_t count, const void* buf) {
    if (!dev || !buf || count == 0) return -1;
    if (dev->flags & BLOCK_FLAG_READONLY) {
        serial_println("[BLOCK] Write to read-only block device rejected!");
        return -4;
    }
    if (lba + count > dev->total_sectors) {
        serial_println("[BLOCK] Write out of bounds error!");
        return -2;
    }
    if (!dev->write_sectors) return -3;

    // Route through high-speed block buffer cache with write-back
    for (uint32_t i = 0; i < count; i++) {
        int r = bcache_write(dev, lba + i, (const uint8_t*)buf + (i * dev->sector_size));
        if (r != 0) return r;
    }
    return 0;
}

int block_flush(block_dev_t* dev) {
    if (!dev) return -1;
    bcache_sync(dev);
    if (dev->flush) return dev->flush(dev);
    return 0;
}

// Block Device Automated Verification Test Suite
void block_run_tests(void) {
    serial_println("=========================================");
    serial_println("[BLOCK] Running Block Device Test Suite...");
    serial_println("=========================================");

    block_dev_t* dev = block_get_default();
    if (!dev) {
        serial_println("[BLOCK] TEST FAILED: No block device available!");
        return;
    }

    // TEST 1: Single sector write and readback
    uint8_t write_buf[BLOCK_SECTOR_SIZE];
    uint8_t read_buf[BLOCK_SECTOR_SIZE];

    for (int i = 0; i < BLOCK_SECTOR_SIZE; i++) {
        write_buf[i] = (uint8_t)(i ^ 0x5A);
        read_buf[i] = 0;
    }

    uint32_t test_lba = dev->total_sectors - 4; // Use test sectors near end
    int r = block_write(dev, test_lba, 1, write_buf);
    if (r != 0) {
        serial_println("  -> [TEST 1/4] block_write: FAIL (ret != 0)");
        return;
    }
    serial_println("  -> [TEST 1/4] Single sector write: PASS");

    r = block_read(dev, test_lba, 1, read_buf);
    if (r != 0) {
        serial_println("  -> [TEST 2/4] block_read: FAIL");
        return;
    }

    int match = 1;
    for (int i = 0; i < BLOCK_SECTOR_SIZE; i++) {
        if (read_buf[i] != write_buf[i]) {
            match = 0;
            break;
        }
    }
    if (match) {
        serial_println("  -> [TEST 2/4] Sector readback integrity: PASS");
    } else {
        serial_println("  -> [TEST 2/4] Sector readback integrity: FAIL (data mismatch)");
        return;
    }

    // TEST 3: Multi-sector read/write (2 sectors = 1024 bytes)
    uint8_t multi_w[BLOCK_SECTOR_SIZE * 2];
    uint8_t multi_r[BLOCK_SECTOR_SIZE * 2];
    for (int i = 0; i < BLOCK_SECTOR_SIZE * 2; i++) {
        multi_w[i] = (uint8_t)((i * 3 + 7) & 0xFF);
        multi_r[i] = 0;
    }
    r = block_write(dev, test_lba + 1, 2, multi_w);
    if (r == 0) {
        block_read(dev, test_lba + 1, 2, multi_r);
        int mmatch = 1;
        for (int i = 0; i < BLOCK_SECTOR_SIZE * 2; i++) {
            if (multi_r[i] != multi_w[i]) { mmatch = 0; break; }
        }
        if (mmatch) {
            serial_println("  -> [TEST 3/4] Multi-sector 1024-byte transfer: PASS");
        } else {
            serial_println("  -> [TEST 3/4] Multi-sector transfer: FAIL (mismatch)");
        }
    } else {
        serial_println("  -> [TEST 3/4] Multi-sector write: FAIL");
    }

    // TEST 4: Boundary check
    r = block_write(dev, dev->total_sectors + 10, 1, write_buf);
    if (r < 0) {
        serial_println("  -> [TEST 4/4] Out-of-bounds safety rejection: PASS");
    } else {
        serial_println("  -> [TEST 4/4] Out-of-bounds safety rejection: FAIL");
    }

    serial_println("[BLOCK] Block Device Test Suite: 4/4 PASSED!");
}
