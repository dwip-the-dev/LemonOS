#ifndef ATA_H
#define ATA_H

#include "block.h"
#include <stdint.h>
#include <stddef.h>

// ATA Ports
#define ATA_PRIMARY_IO_BASE        0x1F0
#define ATA_PRIMARY_CTRL_BASE      0x3F6
#define ATA_SECONDARY_IO_BASE      0x170
#define ATA_SECONDARY_CTRL_BASE    0x376

// Register Offsets
#define ATA_REG_DATA               0x00
#define ATA_REG_ERROR              0x01
#define ATA_REG_FEATURES           0x01
#define ATA_REG_SEC_COUNT          0x02
#define ATA_REG_LBA_LO             0x03
#define ATA_REG_LBA_MID            0x04
#define ATA_REG_LBA_HI             0x05
#define ATA_REG_DRIVE_HEAD         0x06
#define ATA_REG_STATUS             0x07
#define ATA_REG_COMMAND            0x07

// Control Register
#define ATA_REG_DEV_CTRL           0x02 // offset from ctrl base (0x3F6)

// Status Bits
#define ATA_SR_BSY                 0x80
#define ATA_SR_DRDY                0x40
#define ATA_SR_DF                  0x20
#define ATA_SR_DSC                 0x10
#define ATA_SR_DRQ                 0x08
#define ATA_SR_CORR                0x04
#define ATA_SR_IDX                 0x02
#define ATA_SR_ERR                 0x01

// Commands
#define ATA_CMD_READ_PIO           0x20
#define ATA_CMD_WRITE_PIO          0x30
#define ATA_CMD_CACHE_FLUSH        0xE7
#define ATA_CMD_IDENTIFY           0xEC

// ATA Drive Selection
#define ATA_DRIVE_MASTER           0x00
#define ATA_DRIVE_SLAVE            0x01

// API
void ata_init(void);

#endif
