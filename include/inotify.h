#ifndef INOTIFY_H
#define INOTIFY_H

#include <stdint.h>
#include <stddef.h>

#define IN_ACCESS        0x00000001
#define IN_MODIFY        0x00000002
#define IN_ATTRIB        0x00000004
#define IN_CLOSE_WRITE   0x00000008
#define IN_CLOSE_NOWRITE 0x00000010
#define IN_OPEN          0x00000020
#define IN_MOVED_FROM    0x00000040
#define IN_MOVED_TO      0x00000080
#define IN_CREATE        0x00000100
#define IN_DELETE        0x00000200
#define IN_DELETE_SELF   0x00000400
#define IN_MOVE_SELF     0x00000800

#define IN_ALL_EVENTS    0x00000FFF
#define IN_ISDIR         0x40000000

#define INOTIFY_MAX_WATCHES 32
#define INOTIFY_MAX_EVENTS  64
#define INOTIFY_NAME_LEN    64

struct inotify_event {
    int      wd;
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
    char     name[INOTIFY_NAME_LEN];
};

void inotify_init_subsystem(void);
int  inotify_init(void);
int  inotify_add_watch(int fd, const char* pathname, uint32_t mask);
int  inotify_rm_watch(int fd, int wd);
void inotify_trigger(const char* path, uint32_t event_mask);
int  inotify_read_event(int fd, struct inotify_event* event);
int  inotify_get_pending_count(int fd);

// Verification test suite
void inotify_run_tests(void);

#endif
