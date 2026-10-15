#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>
#include <stddef.h>
#include "vfs.h"

// LemonOS UNIX Pipe & IPC Subsystem
// Author: Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>
#define PIPE_BUF_SIZE 4096
#define MAX_PIPES     32

typedef struct pipe {
    uint8_t     buffer[PIPE_BUF_SIZE];
    uint32_t    head;       // Write cursor
    uint32_t    tail;       // Read cursor
    uint32_t    count;      // Available bytes to read
    int         readers;    // Open readers count
    int         writers;    // Open writers count
    int         in_use;
    vfs_node_t  read_node;
    vfs_node_t  write_node;
} pipe_t;

// Public Pipe API
void pipe_init(void);
int  pipe_create(vfs_node_t** out_read_node, vfs_node_t** out_write_node);
int  sys_pipe(int pipefd[2]);
int  pipe_read(vfs_node_t* node, uint32_t offset, uint32_t size, void* buf);
int  pipe_write(vfs_node_t* node, uint32_t offset, uint32_t size, const void* buf);
int  pipe_close(vfs_node_t* node);
void pipe_run_tests(void);

#endif // PIPE_H
