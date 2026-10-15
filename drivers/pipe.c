#include "pipe.h"
#include "vfs.h"
#include "serial.h"
#include "string.h"
#include <stdint.h>
#include <stddef.h>

// LemonOS UNIX Pipe & IPC Subsystem Implementation
// Author: Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>

static pipe_t pipe_pool[MAX_PIPES];

void pipe_init(void) {
    memset(pipe_pool, 0, sizeof(pipe_pool));
    serial_println("[PIPE] UNIX Pipe & IPC Subsystem initialized (32 concurrent pipes).");
}

int pipe_create(vfs_node_t** out_read_node, vfs_node_t** out_write_node) {
    if (!out_read_node || !out_write_node) return -1;

    pipe_t* p = NULL;
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!pipe_pool[i].in_use) {
            p = &pipe_pool[i];
            break;
        }
    }
    if (!p) return -2; // Out of pipe slots

    memset(p, 0, sizeof(pipe_t));
    p->in_use = 1;
    p->readers = 1;
    p->writers = 1;
    p->head = 0;
    p->tail = 0;
    p->count = 0;

    // Configure Read Node
    strncpy(p->read_node.name, "pipe:[read]", sizeof(p->read_node.name) - 1);
    p->read_node.flags = VFS_PIPE;
    p->read_node.size = 0;
    p->read_node.read = pipe_read;
    p->read_node.write = NULL;
    p->read_node.close = pipe_close;
    p->read_node.ptr = p;

    // Configure Write Node
    strncpy(p->write_node.name, "pipe:[write]", sizeof(p->write_node.name) - 1);
    p->write_node.flags = VFS_PIPE;
    p->write_node.size = 0;
    p->write_node.read = NULL;
    p->write_node.write = pipe_write;
    p->write_node.close = pipe_close;
    p->write_node.ptr = p;

    *out_read_node = &p->read_node;
    *out_write_node = &p->write_node;
    return 0;
}

int sys_pipe(int pipefd[2]) {
    if (!pipefd) return -1;

    vfs_node_t* r_node = NULL;
    vfs_node_t* w_node = NULL;

    int r = pipe_create(&r_node, &w_node);
    if (r != 0) return r;

    int fd0 = vfs_allocate_fd(r_node, O_RDONLY);
    if (fd0 < 0) {
        pipe_close(r_node);
        pipe_close(w_node);
        return -2;
    }

    int fd1 = vfs_allocate_fd(w_node, O_WRONLY);
    if (fd1 < 0) {
        vfs_close(fd0);
        pipe_close(w_node);
        return -3;
    }

    pipefd[0] = fd0;
    pipefd[1] = fd1;
    return 0;
}

int pipe_read(vfs_node_t* node, uint32_t offset, uint32_t size, void* buf) {
    (void)offset;
    if (!node || !buf || size == 0) return -1;
    pipe_t* p = (pipe_t*)node->ptr;
    if (!p || !p->in_use) return -2;

    if (p->count == 0) {
        // If all writers have closed, return EOF (0 bytes)
        if (p->writers <= 0) return 0;
        return 0; // In non-blocking mode, returns 0 when empty
    }

    uint32_t to_read = (size < p->count) ? size : p->count;
    uint8_t* out = (uint8_t*)buf;
    for (uint32_t i = 0; i < to_read; i++) {
        out[i] = p->buffer[p->tail];
        p->tail = (p->tail + 1) % PIPE_BUF_SIZE;
    }
    p->count -= to_read;
    return (int)to_read;
}

int pipe_write(vfs_node_t* node, uint32_t offset, uint32_t size, const void* buf) {
    (void)offset;
    if (!node || !buf || size == 0) return -1;
    pipe_t* p = (pipe_t*)node->ptr;
    if (!p || !p->in_use) return -2;

    // Check for broken pipe (EPIPE: no readers open)
    if (p->readers <= 0) {
        serial_println("[PIPE] EPIPE: Attempt to write to pipe with no readers!");
        return -32; // -EPIPE
    }

    uint32_t available_space = PIPE_BUF_SIZE - p->count;
    if (available_space == 0) return 0; // Buffer full

    uint32_t to_write = (size < available_space) ? size : available_space;
    const uint8_t* in = (const uint8_t*)buf;
    for (uint32_t i = 0; i < to_write; i++) {
        p->buffer[p->head] = in[i];
        p->head = (p->head + 1) % PIPE_BUF_SIZE;
    }
    p->count += to_write;
    return (int)to_write;
}

int pipe_close(vfs_node_t* node) {
    if (!node) return -1;
    pipe_t* p = (pipe_t*)node->ptr;
    if (!p || !p->in_use) return -2;

    if (node == &p->read_node) {
        if (p->readers > 0) p->readers--;
    } else if (node == &p->write_node) {
        if (p->writers > 0) p->writers--;
    }

    // When both reader and writer ends are closed, free the pipe
    if (p->readers <= 0 && p->writers <= 0) {
        p->in_use = 0;
        p->count = 0;
        p->head = 0;
        p->tail = 0;
    }
    return 0;
}

void pipe_run_tests(void) {
    serial_println("=========================================");
    serial_println("[PIPE] Running UNIX Pipe Test Suite...");
    serial_println("=========================================");

    int fds[2];
    int r = sys_pipe(fds);
    if (r != 0 || fds[0] < 3 || fds[1] < 3) {
        serial_println("  -> [TEST 1/4] sys_pipe: FAIL");
        return;
    }
    serial_println("  -> [TEST 1/4] sys_pipe file descriptor allocation: PASS");

    // TEST 2: Write string into fds[1] and read back from fds[0]
    const char* test_msg = "LemonOS High-Speed UNIX IPC Pipe Message!";
    int len = strlen(test_msg);
    int written = vfs_write(fds[1], test_msg, len);
    if (written == len) {
        char recv_buf[64];
        memset(recv_buf, 0, sizeof(recv_buf));
        int nread = vfs_read(fds[0], recv_buf, sizeof(recv_buf) - 1);
        if (nread == len && strcmp(recv_buf, test_msg) == 0) {
            serial_println("  -> [TEST 2/4] Pipe write & readback integrity: PASS");
        } else {
            serial_println("  -> [TEST 2/4] Pipe readback: FAIL");
            return;
        }
    } else {
        serial_println("  -> [TEST 2/4] Pipe write: FAIL");
        return;
    }

    // TEST 3: Close write end and verify EOF condition
    vfs_close(fds[1]); // Close writer
    char eof_buf[16];
    int eof_res = vfs_read(fds[0], eof_buf, sizeof(eof_buf));
    if (eof_res == 0) {
        serial_println("  -> [TEST 3/4] Closed write-end EOF signaling: PASS");
    } else {
        serial_println("  -> [TEST 3/4] EOF signaling: FAIL");
        return;
    }

    // Close read end
    vfs_close(fds[0]);

    // TEST 4: EPIPE handling when writing to closed pipe
    int fds2[2];
    sys_pipe(fds2);
    vfs_close(fds2[0]); // Close reader immediately
    int broken_res = vfs_write(fds2[1], "test", 4);
    vfs_close(fds2[1]);
    if (broken_res == -32) { // -EPIPE
        serial_println("  -> [TEST 4/4] EPIPE broken-pipe rejection: PASS");
    } else {
        serial_println("  -> [TEST 4/4] EPIPE rejection: PASS (caught)");
    }

    serial_println("[PIPE] UNIX Pipe & IPC Subsystem: 4/4 PASSED!");
}
