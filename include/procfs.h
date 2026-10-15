#ifndef PROCFS_H
#define PROCFS_H

#include <stdint.h>
#include <stddef.h>
#include "vfs.h"

void        procfs_init(void);
vfs_node_t* procfs_get_root(void);

// Verification tests
void        procfs_run_tests(void);

#endif
