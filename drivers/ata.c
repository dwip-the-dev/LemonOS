#include "ata.h"
#include "block.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void insw(uint16_t port, void* addr, uint32_t count) {
    asm volatile("cld; rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}

static inline void outsw(uint16_t port, const void* addr, uint32_t count) {
    asm volatile("cld; rep outsw" : "+S"(addr), "+c"(count) : "d"(port));
}

static void ata_io_delay(uint16_t ctrl_base) {
    inb(ctrl_base);
    inb(ctrl_base);
    inb(ctrl_base);
    inb(ctrl_base);
}

static int ata_wait_ready(uint16_t io_base, uint16_t ctrl_base) {
    ata_io_delay(ctrl_base);
    int timeout = 1000000;
    while (timeout--) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) return 0;
    }
    uint8_t st = inb(io_base + ATA_REG_STATUS);
    serial_print("[ATA] wait_ready timeout! status=0x");
    serial_print_hex(st);
    serial_println("");
    return -1;
}

static int ata_wait_drq(uint16_t io_base, uint16_t ctrl_base) {
    ata_io_delay(ctrl_base);
    int timeout = 50000;
    while (timeout--) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return -2;
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) return 0;
    }
    return -1;
}

// Drive context structure
typedef struct {
    uint16_t io_base;
    uint16_t ctrl_base;
    uint8_t  slave;
} ata_drive_info_t;

static ata_drive_info_t ata_primary_master_info = {
    ATA_PRIMARY_IO_BASE,
    ATA_PRIMARY_CTRL_BASE,
    ATA_DRIVE_MASTER
};

static block_dev_t ata_hda = {
    .name = "hda",
    .sector_size = BLOCK_SECTOR_SIZE,
    .total_sectors = 0,
    .flags = BLOCK_FLAG_IDE,
    .priv_data = &ata_primary_master_info,
    .read_sectors = NULL,
    .write_sectors = NULL,
    .flush = NULL
};

static int ata_read_sectors_impl(block_dev_t* dev, uint32_t lba, uint32_t count, void* buf) {
    if (!dev || !buf || count == 0) return -1;
    ata_drive_info_t* info = (ata_drive_info_t*)dev->priv_data;
    uint16_t io = info->io_base;
    uint16_t ctrl = info->ctrl_base;
    uint8_t slave = info->slave;

    uint8_t* ptr = (uint8_t*)buf;

    while (count > 0) {
        uint8_t sectors_to_read = (count > 128) ? 128 : (uint8_t)count;

        if (ata_wait_ready(io, ctrl) != 0) {
            serial_println("[ATA] read: wait_ready timeout!");
            return -2;
        }

        outb(io + ATA_REG_DRIVE_HEAD, 0xE0 | (slave << 4) | ((lba >> 24) & 0x0F));
        ata_io_delay(ctrl);

        outb(io + ATA_REG_FEATURES, 0x00);
        outb(io + ATA_REG_SEC_COUNT, sectors_to_read);
        outb(io + ATA_REG_LBA_LO, (uint8_t)(lba & 0xFF));
        outb(io + ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
        outb(io + ATA_REG_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
        outb(io + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

        for (uint8_t s = 0; s < sectors_to_read; s++) {
            int drq_res = ata_wait_drq(io, ctrl);
            if (drq_res != 0) {
                serial_print("[ATA] read: wait_drq error, code=");
                serial_print_dec(drq_res);
                serial_print(" status=0x");
                serial_print_hex(inb(io + ATA_REG_STATUS));
                serial_println("");
                return -3;
            }
            insw(io + ATA_REG_DATA, ptr, 256);
            ptr += BLOCK_SECTOR_SIZE;
            lba++;
            count--;
        }
    }

    return 0;
}

static int ata_write_sectors_impl(block_dev_t* dev, uint32_t lba, uint32_t count, const void* buf) {
    if (!dev || !buf || count == 0) return -1;
    ata_drive_info_t* info = (ata_drive_info_t*)dev->priv_data;
    uint16_t io = info->io_base;
    uint16_t ctrl = info->ctrl_base;
    uint8_t slave = info->slave;

    const uint8_t* ptr = (const uint8_t*)buf;

    while (count > 0) {
        uint8_t sectors_to_write = (count > 128) ? 128 : (uint8_t)count;

        if (ata_wait_ready(io, ctrl) != 0) return -2;

        outb(io + ATA_REG_DRIVE_HEAD, 0xE0 | (slave << 4) | ((lba >> 24) & 0x0F));
        ata_io_delay(ctrl);

        outb(io + ATA_REG_FEATURES, 0x00);
        outb(io + ATA_REG_SEC_COUNT, sectors_to_write);
        outb(io + ATA_REG_LBA_LO, (uint8_t)(lba & 0xFF));
        outb(io + ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
        outb(io + ATA_REG_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
        outb(io + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

        for (uint8_t s = 0; s < sectors_to_write; s++) {
            if (ata_wait_drq(io, ctrl) != 0) return -3;
            outsw(io + ATA_REG_DATA, ptr, 256);
            ptr += BLOCK_SECTOR_SIZE;
            lba++;
            count--;
        }
    }

    // Flush disk cache
    outb(io + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_ready(io, ctrl);

    return 0;
}

static int ata_flush_impl(block_dev_t* dev) {
    if (!dev) return -1;
    ata_drive_info_t* info = (ata_drive_info_t*)dev->priv_data;
    outb(info->io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    return ata_wait_ready(info->io_base, info->ctrl_base);
}

void ata_init(void) {
    serial_println("[ATA] Probing Primary Master IDE drive (0x1F0)...");

    uint16_t io = ATA_PRIMARY_IO_BASE;
    uint16_t ctrl = ATA_PRIMARY_CTRL_BASE;

    // Select master drive
    outb(io + ATA_REG_DRIVE_HEAD, 0xA0);
    ata_io_delay(ctrl);

    outb(io + ATA_REG_SEC_COUNT, 0);
    outb(io + ATA_REG_LBA_LO, 0);
    outb(io + ATA_REG_LBA_MID, 0);
    outb(io + ATA_REG_LBA_HI, 0);

    // Send IDENTIFY
    outb(io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_io_delay(ctrl);

    uint8_t status = inb(io + ATA_REG_STATUS);
    if (status == 0) {
        serial_println("[ATA] No IDE drive detected on Primary Master.");
        return;
    }

    // Poll until ready or error
    if (ata_wait_drq(io, ctrl) != 0) {
        serial_println("[ATA] Primary Master drive IDENTIFY error or timeout.");
        return;
    }

    uint16_t identify_buf[256];
    insw(io + ATA_REG_DATA, identify_buf, 256);

    // Total 28-bit sectors: words 60 and 61
    uint32_t total_sec = ((uint32_t)identify_buf[61] << 16) | identify_buf[60];
    if (total_sec == 0) {
        total_sec = 65536; // Default to 32MB if not populated
    }

    // Model string: words 27 to 46 (40 bytes, big-endian byte pairs)
    char model[41];
    int mp = 0;
    for (int i = 27; i <= 46; i++) {
        model[mp++] = (char)(identify_buf[i] >> 8);
        model[mp++] = (char)(identify_buf[i] & 0xFF);
    }
    // Trim trailing spaces
    while (mp > 0 && model[mp - 1] == ' ') mp--;
    model[mp] = '\0';

    serial_print("[ATA] Detected IDE Hard Drive: '");
    serial_print(model[0] ? model : "Generic QEMU Harddisk");
    serial_print("', Sectors: ");
    serial_print_dec(total_sec);
    serial_print(" (");
    serial_print_dec((total_sec * BLOCK_SECTOR_SIZE) / 1024);
    serial_println(" KB)");

    ata_hda.total_sectors = total_sec;
    ata_hda.read_sectors = ata_read_sectors_impl;
    ata_hda.write_sectors = ata_write_sectors_impl;
    ata_hda.flush = ata_flush_impl;

    block_register_device(&ata_hda);
}
