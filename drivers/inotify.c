#include "inotify.h"
#include "string.h"
#include "serial.h"

typedef struct {
    int      wd;
    int      fd;
    char     path[128];
    uint32_t mask;
    int      active;
} inotify_watch_entry_t;

static inotify_watch_entry_t watches[INOTIFY_MAX_WATCHES];
static struct inotify_event event_queue[INOTIFY_MAX_EVENTS];
static int queue_head = 0;
static int queue_tail = 0;
static int queue_count = 0;

static int next_wd = 1;
static int next_fd = 100;

void inotify_init_subsystem(void) {
    memset(watches, 0, sizeof(watches));
    memset(event_queue, 0, sizeof(event_queue));
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    next_wd = 1;
    next_fd = 100;
    serial_println("[INOTIFY] Filesystem notification subsystem initialized.");
}

int inotify_init(void) {
    int fd = next_fd++;
    return fd;
}

int inotify_add_watch(int fd, const char* pathname, uint32_t mask) {
    if (!pathname || pathname[0] == '\0') return -1;

    // Check if watch already exists for this path on fd
    for (int i = 0; i < INOTIFY_MAX_WATCHES; i++) {
        if (watches[i].active && watches[i].fd == fd && strcmp(watches[i].path, pathname) == 0) {
            watches[i].mask = mask;
            return watches[i].wd;
        }
    }

    // Allocate new watch slot
    for (int i = 0; i < INOTIFY_MAX_WATCHES; i++) {
        if (!watches[i].active) {
            watches[i].wd = next_wd++;
            watches[i].fd = fd;
            strncpy(watches[i].path, pathname, sizeof(watches[i].path) - 1);
            watches[i].path[sizeof(watches[i].path) - 1] = '\0';
            watches[i].mask = mask;
            watches[i].active = 1;
            return watches[i].wd;
        }
    }
    return -1; // Out of watch descriptors
}

int inotify_rm_watch(int fd, int wd) {
    for (int i = 0; i < INOTIFY_MAX_WATCHES; i++) {
        if (watches[i].active && watches[i].fd == fd && watches[i].wd == wd) {
            watches[i].active = 0;
            return 0;
        }
    }
    return -1;
}

void inotify_trigger(const char* path, uint32_t event_mask) {
    if (!path || path[0] == '\0') return;

    for (int i = 0; i < INOTIFY_MAX_WATCHES; i++) {
        if (!watches[i].active) continue;

        // Check if event mask matches watch mask
        if (!(watches[i].mask & event_mask)) continue;

        int match = 0;
        const char* event_name = "";

        // Exact match
        if (strcmp(watches[i].path, path) == 0) {
            match = 1;
            event_name = "";
        } else {
            // Check directory prefix
            size_t wlen = strlen(watches[i].path);
            if (strncmp(watches[i].path, path, wlen) == 0) {
                if (path[wlen] == '/') {
                    match = 1;
                    event_name = path + wlen + 1;
                } else if (wlen == 1 && watches[i].path[0] == '/') {
                    match = 1;
                    event_name = path + 1;
                }
            }
        }

        if (match && queue_count < INOTIFY_MAX_EVENTS) {
            struct inotify_event* ev = &event_queue[queue_tail];
            ev->wd = watches[i].wd;
            ev->mask = event_mask;
            ev->cookie = 0;
            strncpy(ev->name, event_name, INOTIFY_NAME_LEN - 1);
            ev->name[INOTIFY_NAME_LEN - 1] = '\0';
            ev->len = (uint32_t)strlen(ev->name);

            queue_tail = (queue_tail + 1) % INOTIFY_MAX_EVENTS;
            queue_count++;
        }
    }
}

int inotify_read_event(int fd, struct inotify_event* event) {
    if (!event || queue_count == 0) return 0;

    // Search for event belonging to this fd
    int inspected = 0;
    int curr = queue_head;
    while (inspected < queue_count) {
        int wd = event_queue[curr].wd;
        int belongs_to_fd = 0;
        for (int i = 0; i < INOTIFY_MAX_WATCHES; i++) {
            if (watches[i].active && watches[i].wd == wd && watches[i].fd == fd) {
                belongs_to_fd = 1;
                break;
            }
        }

        if (belongs_to_fd) {
            *event = event_queue[curr];
            // Shift queue forward if it was the head
            if (curr == queue_head) {
                queue_head = (queue_head + 1) % INOTIFY_MAX_EVENTS;
                queue_count--;
            }
            return 1;
        }
        curr = (curr + 1) % INOTIFY_MAX_EVENTS;
        inspected++;
    }
    return 0;
}

int inotify_get_pending_count(int fd) {
    int count = 0;
    int curr = queue_head;
    for (int i = 0; i < queue_count; i++) {
        int wd = event_queue[curr].wd;
        for (int w = 0; w < INOTIFY_MAX_WATCHES; w++) {
            if (watches[w].active && watches[w].wd == wd && watches[w].fd == fd) {
                count++;
                break;
            }
        }
        curr = (curr + 1) % INOTIFY_MAX_EVENTS;
    }
    return count;
}

void inotify_run_tests(void) {
    serial_println("[INOTIFY] Running Inotify Verification Tests...");
    int ifd = inotify_init();
    if (ifd < 0) {
        serial_println("  -> [TEST 1/2] inotify_init: FAIL");
        return;
    }
    serial_println("  -> [TEST 1/2] inotify_init: PASS");

    int wd = inotify_add_watch(ifd, "/docs", IN_CREATE | IN_MODIFY | IN_DELETE);
    if (wd > 0) {
        inotify_trigger("/docs/test.txt", IN_CREATE);
        struct inotify_event ev;
        if (inotify_read_event(ifd, &ev) && ev.wd == wd && (ev.mask & IN_CREATE)) {
            serial_println("  -> [TEST 2/2] inotify event delivery (IN_CREATE): PASS");
        } else {
            serial_println("  -> [TEST 2/2] inotify event delivery: FAIL");
        }
    } else {
        serial_println("  -> [TEST 2/2] inotify_add_watch: FAIL");
    }
    inotify_rm_watch(ifd, wd);
    serial_println("[INOTIFY] Inotify verification complete.");
}
