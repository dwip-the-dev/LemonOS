#ifndef DEVFS_H
#define DEVFS_H

#include "vfs.h"

// Initialize DevFS and register standard device nodes (/dev/null, /dev/zero, /dev/urandom, /dev/tty, /dev/hda, /dev/ramdisk0)
void        devfs_init(void);
vfs_node_t* devfs_get_root(void);

#endif
