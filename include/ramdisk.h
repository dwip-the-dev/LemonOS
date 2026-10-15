#ifndef RAMDISK_H
#define RAMDISK_H

#include "block.h"
#include <stdint.h>
#include <stddef.h>

#define RAMDISK_SECTORS  4096 // 2 MB ramdisk (4096 * 512)

void ramdisk_init(void);

#endif
