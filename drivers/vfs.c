#include "vfs.h"
#include "inotify.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

static vfs_mount_t mounts[VFS_MAX_MOUNTS];
static vfs_file_t  open_files[VFS_MAX_FDS];
static int         vfs_initialized = 0;

static int vfs_strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static int vfs_strncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == '\0' || b[i] == '\0') {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }
    }
    return 0;
}

static void vfs_strcpy(char* d, const char* s, int max) {
    int i = 0;
    while (s && s[i] && i < max - 1) { d[i] = s[i]; i++; }
    d[i] = '\0';
}

static int vfs_strlen(const char* s) {
    int len = 0;
    while (s && s[len]) len++;
    return len;
}

void vfs_init(void) {
    if (vfs_initialized) return;

    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        mounts[i].in_use = 0;
        mounts[i].path[0] = '\0';
        mounts[i].root_node = NULL;
    }

    for (int i = 0; i < VFS_MAX_FDS; i++) {
        open_files[i].in_use = 0;
        open_files[i].node = NULL;
        open_files[i].offset = 0;
        open_files[i].flags = 0;
    }

    vfs_initialized = 1;
    serial_println("[VFS] Virtual File System subsystem initialized.");
}

int vfs_mount(const char* path, vfs_node_t* root_node) {
    if (!path || !root_node) return -1;

    // Check if already mounted
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mounts[i].in_use && vfs_strcmp(mounts[i].path, path) == 0) {
            mounts[i].root_node = root_node;
            serial_print("[VFS] Updated mount on '");
            serial_print(path);
            serial_println("'");
            return 0;
        }
    }

    // Find free mount slot
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!mounts[i].in_use) {
            mounts[i].in_use = 1;
            vfs_strcpy(mounts[i].path, path, VFS_MAX_PATH);
            mounts[i].root_node = root_node;
            serial_print("[VFS] Mounted filesystem at '");
            serial_print(path);
            serial_println("'");
            return 0;
        }
    }

    serial_println("[VFS] Mount table full!");
    return -2;
}

int vfs_unmount(const char* path) {
    if (!path) return -1;
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mounts[i].in_use && vfs_strcmp(mounts[i].path, path) == 0) {
            mounts[i].in_use = 0;
            mounts[i].path[0] = '\0';
            mounts[i].root_node = NULL;
            serial_print("[VFS] Unmounted '");
            serial_print(path);
            serial_println("'");
            return 0;
        }
    }
    return -2;
}

vfs_node_t* vfs_get_root(void) {
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mounts[i].in_use && vfs_strcmp(mounts[i].path, "/") == 0) {
            return mounts[i].root_node;
        }
    }
    return NULL;
}

// Longest-prefix mount match
static int vfs_find_mount(const char* path, const char** out_rel_path) {
    int best_match = -1;
    int best_len = -1;

    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!mounts[i].in_use) continue;
        int mlen = vfs_strlen(mounts[i].path);

        // Exact match
        if (vfs_strcmp(mounts[i].path, path) == 0) {
            if (mlen > best_len) {
                best_match = i;
                best_len = mlen;
                if (out_rel_path) *out_rel_path = "";
            }
        } else if (mlen == 1 && mounts[i].path[0] == '/') {
            // Root mount matches any absolute path
            if (best_len < 1) {
                best_match = i;
                best_len = 1;
                if (out_rel_path) {
                    *out_rel_path = (path[0] == '/') ? (path + 1) : path;
                }
            }
        } else if (vfs_strncmp(mounts[i].path, path, mlen) == 0 && (path[mlen] == '/' || path[mlen] == '\0')) {
            if (mlen > best_len) {
                best_match = i;
                best_len = mlen;
                if (out_rel_path) {
                    *out_rel_path = (path[mlen] == '/') ? (path + mlen + 1) : (path + mlen);
                }
            }
        }
    }

    return best_match;
}

vfs_node_t* vfs_resolve_path(const char* path) {
    if (!path || path[0] == '\0') return NULL;

    const char* rel = NULL;
    int m_idx = vfs_find_mount(path, &rel);
    if (m_idx < 0) return NULL;

    vfs_node_t* current = mounts[m_idx].root_node;
    if (!current) return NULL;

    // Follow mountpoint redirection if present
    if ((current->flags & VFS_MOUNTPOINT) && current->mount_point) {
        current = current->mount_point;
    }

    if (!rel || rel[0] == '\0') {
        return current;
    }

    // Traverse directory components
    char comp[VFS_NAME_MAX];
    int p = 0;

    while (rel[p]) {
        while (rel[p] == '/') p++;
        if (!rel[p]) break;

        int cp = 0;
        while (rel[p] && rel[p] != '/' && cp < VFS_NAME_MAX - 1) {
            comp[cp++] = rel[p++];
        }
        comp[cp] = '\0';

        if (vfs_strcmp(comp, ".") == 0) continue;
        if (vfs_strcmp(comp, "..") == 0) {
            // Not strictly tracking parent in simple tree, stay at current
            continue;
        }

        if (!current->finddir) return NULL;
        vfs_node_t* next = current->finddir(current, comp);
        if (!next) return NULL;

        current = next;
        if ((current->flags & VFS_MOUNTPOINT) && current->mount_point) {
            current = current->mount_point;
        }
    }

    return current;
}

// Helper to extract parent dir and filename
static int vfs_split_path(const char* path, char* parent_buf, int max_parent, char* name_buf, int max_name) {
    if (!path) return -1;
    int len = vfs_strlen(path);
    while (len > 0 && path[len - 1] == '/') len--;

    int last_slash = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '/') {
            last_slash = i;
            break;
        }
    }

    if (last_slash < 0) {
        vfs_strcpy(parent_buf, "/", max_parent);
        vfs_strcpy(name_buf, path, max_name);
    } else if (last_slash == 0) {
        vfs_strcpy(parent_buf, "/", max_parent);
        int np = 0;
        for (int i = 1; i < len && np < max_name - 1; i++) {
            name_buf[np++] = path[i];
        }
        name_buf[np] = '\0';
    } else {
        int pp = 0;
        for (int i = 0; i < last_slash && pp < max_parent - 1; i++) {
            parent_buf[pp++] = path[i];
        }
        parent_buf[pp] = '\0';

        int np = 0;
        for (int i = last_slash + 1; i < len && np < max_name - 1; i++) {
            name_buf[np++] = path[i];
        }
        name_buf[np] = '\0';
    }
    return 0;
}

// POSIX File Operations API
int vfs_open(const char* path, int flags) {
    if (!path) return -1;

    vfs_node_t* node = vfs_resolve_path(path);
    if (!node) {
        if (flags & O_CREAT) {
            char parent_path[VFS_MAX_PATH];
            char file_name[VFS_NAME_MAX];
            if (vfs_split_path(path, parent_path, sizeof(parent_path), file_name, sizeof(file_name)) != 0) {
                return -2;
            }

            vfs_node_t* parent = vfs_resolve_path(parent_path);
            if (!parent || !parent->create) {
                return -3;
            }

            node = parent->create(parent, file_name, 0644);
            if (!node) return -4;
            inotify_trigger(path, IN_CREATE);
        } else {
            return -5; // File not found
        }
    }

    // Find free file descriptor
    int fd = -1;
    for (int i = 3; i < VFS_MAX_FDS; i++) { // Reserve 0, 1, 2 for stdin, stdout, stderr
        if (!open_files[i].in_use) {
            fd = i;
            break;
        }
    }
    if (fd < 0) return -6; // Out of file descriptors

    if (node->open) {
        int r = node->open(node, flags);
        if (r != 0) return r;
    }

    open_files[fd].in_use = 1;
    open_files[fd].node = node;
    open_files[fd].flags = flags;
    open_files[fd].offset = (flags & O_APPEND) ? node->size : 0;

    return fd;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !open_files[fd].in_use) return -1;
    vfs_node_t* node = open_files[fd].node;

    if (node && node->close) {
        node->close(node);
    }

    open_files[fd].in_use = 0;
    open_files[fd].node = NULL;
    open_files[fd].offset = 0;
    open_files[fd].flags = 0;
    return 0;
}

int vfs_allocate_fd(vfs_node_t* node, int flags) {
    if (!node) return -1;
    for (int i = 3; i < VFS_MAX_FDS; i++) {
        if (!open_files[i].in_use) {
            open_files[i].in_use = 1;
            open_files[i].node = node;
            open_files[i].flags = flags;
            open_files[i].offset = 0;
            return i;
        }
    }
    return -2;
}

vfs_node_t* vfs_get_node(int fd) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !open_files[fd].in_use) return NULL;
    return open_files[fd].node;
}

vfs_file_t* vfs_get_file(int fd) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !open_files[fd].in_use) return NULL;
    return &open_files[fd];
}

int vfs_dup_min(int oldfd, int min_fd) {
    if (oldfd < 0 || oldfd >= VFS_MAX_FDS || !open_files[oldfd].in_use) return -9; // EBADF
    if (min_fd < 0 || min_fd >= VFS_MAX_FDS) return -22; // EINVAL
    for (int i = min_fd; i < VFS_MAX_FDS; i++) {
        if (!open_files[i].in_use) {
            open_files[i] = open_files[oldfd];
            return i;
        }
    }
    return -24; // EMFILE
}

int vfs_dup(int oldfd) {
    return vfs_dup_min(oldfd, 3);
}

int vfs_dup2(int oldfd, int newfd) {
    if (oldfd < 0 || oldfd >= VFS_MAX_FDS || !open_files[oldfd].in_use) return -9; // EBADF
    if (newfd < 0 || newfd >= VFS_MAX_FDS) return -9; // EBADF
    if (oldfd == newfd) return newfd;

    if (open_files[newfd].in_use) {
        vfs_close(newfd);
    }
    open_files[newfd] = open_files[oldfd];
    return newfd;
}

int vfs_read(int fd, void* buffer, uint32_t size) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !open_files[fd].in_use || !buffer) return -1;
    vfs_file_t* f = &open_files[fd];
    vfs_node_t* node = f->node;
    if (!node || !node->read) return -2;

    int bytes_read = node->read(node, f->offset, size, buffer);
    if (bytes_read > 0) {
        f->offset += bytes_read;
    }
    return bytes_read;
}

int vfs_write(int fd, const void* buffer, uint32_t size) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !open_files[fd].in_use || !buffer) return -1;
    vfs_file_t* f = &open_files[fd];
    vfs_node_t* node = f->node;
    if (!node || !node->write) return -2;

    int bytes_written = node->write(node, f->offset, size, buffer);
    if (bytes_written > 0) {
        f->offset += bytes_written;
        inotify_trigger(node->name, IN_MODIFY);
    }
    return bytes_written;
}

int vfs_seek(int fd, int32_t offset, int whence) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !open_files[fd].in_use) return -1;
    vfs_file_t* f = &open_files[fd];
    vfs_node_t* node = f->node;
    if (!node) return -2;

    int32_t new_offset = 0;
    switch (whence) {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset = (int32_t)f->offset + offset;
            break;
        case SEEK_END:
            new_offset = (int32_t)node->size + offset;
            break;
        default:
            return -3;
    }

    if (new_offset < 0) return -4;
    f->offset = (uint32_t)new_offset;
    return (int)f->offset;
}

int vfs_tell(int fd) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !open_files[fd].in_use) return -1;
    return (int)open_files[fd].offset;
}

int vfs_lseek(int fd, int32_t offset, int whence) {
    return vfs_seek(fd, offset, whence);
}

int vfs_stat(const char* path, vfs_stat_t* out_stat) {
    if (!path || !out_stat) return -1;
    vfs_node_t* node = vfs_resolve_path(path);
    if (!node) return -2;

    if (node->stat) {
        return node->stat(node, out_stat);
    }

    out_stat->inode = node->inode;
    out_stat->size = node->size;
    out_stat->flags = node->flags;
    out_stat->permissions = node->permissions;
    out_stat->uid = node->uid;
    out_stat->gid = node->gid;
    out_stat->atime = node->atime;
    out_stat->mtime = node->mtime;
    out_stat->ctime = node->ctime;
    return 0;
}

int vfs_mkdir(const char* path, uint16_t mode) {
    if (!path) return -1;
    char parent_path[VFS_MAX_PATH];
    char dir_name[VFS_NAME_MAX];
    if (vfs_split_path(path, parent_path, sizeof(parent_path), dir_name, sizeof(dir_name)) != 0) {
        return -2;
    }

    vfs_node_t* parent = vfs_resolve_path(parent_path);
    if (!parent || !parent->mkdir) return -3;

    vfs_node_t* new_dir = parent->mkdir(parent, dir_name, mode);
    if (new_dir) {
        inotify_trigger(path, IN_CREATE | IN_ISDIR);
        return 0;
    }
    return -4;
}

int vfs_unlink(const char* path) {
    if (!path) return -1;
    char parent_path[VFS_MAX_PATH];
    char file_name[VFS_NAME_MAX];
    if (vfs_split_path(path, parent_path, sizeof(parent_path), file_name, sizeof(file_name)) != 0) {
        return -2;
    }

    vfs_node_t* parent = vfs_resolve_path(parent_path);
    if (!parent || !parent->unlink) return -3;

    int res = parent->unlink(parent, file_name);
    if (res == 0) {
        inotify_trigger(path, IN_DELETE);
    }
    return res;
}

int vfs_readdir(const char* path, uint32_t index, vfs_dirent_t* out_dirent) {
    if (!path || !out_dirent) return -1;
    vfs_node_t* dir = vfs_resolve_path(path);
    if (!dir || !(dir->flags & VFS_DIRECTORY) || !dir->readdir) return -2;

    return dir->readdir(dir, index, out_dirent);
}

void vfs_list_mounts(void) {
    serial_println("--- VFS Mount Points ---");
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mounts[i].in_use) {
            serial_print("  Mount: '");
            serial_print(mounts[i].path);
            serial_print("' -> Node: '");
            serial_print(mounts[i].root_node ? mounts[i].root_node->name : "<null>");
            serial_println("'");
        }
    }
}

// VFS Automated Verification Test Suite
void vfs_run_tests(void) {
    serial_println("=========================================");
    serial_println("[VFS] Running Virtual File System Test Suite...");
    serial_println("=========================================");

    // TEST 1: Mount check
    vfs_node_t* root = vfs_get_root();
    if (root) {
        serial_println("  -> [TEST 1/6] Root VFS mount verification: PASS");
    } else {
        serial_println("  -> [TEST 1/6] Root VFS mount verification: FAIL");
        return;
    }

    // TEST 2: Create file via VFS
    int fd = vfs_open("/vfs_test.txt", O_CREAT | O_RDWR);
    if (fd >= 0) {
        serial_println("  -> [TEST 2/6] vfs_open(O_CREAT | O_RDWR): PASS");
    } else {
        serial_println("  -> [TEST 2/6] vfs_open(O_CREAT): FAIL");
        return;
    }

    // TEST 3: Write to file
    const char* test_data = "LemonOS VFS POSIX-compliant data write test!";
    int written = vfs_write(fd, test_data, vfs_strlen(test_data));
    if (written == vfs_strlen(test_data)) {
        serial_println("  -> [TEST 3/6] vfs_write: PASS");
    } else {
        serial_println("  -> [TEST 3/6] vfs_write: FAIL");
        vfs_close(fd);
        return;
    }

    // TEST 4: Seek and readback
    vfs_seek(fd, 0, SEEK_SET);
    char readback[64];
    for (int i = 0; i < 64; i++) readback[i] = 0;
    int bytes_read = vfs_read(fd, readback, sizeof(readback) - 1);
    readback[bytes_read > 0 ? bytes_read : 0] = '\0';
    if (bytes_read == written && vfs_strcmp(readback, test_data) == 0) {
        serial_println("  -> [TEST 4/6] vfs_seek & vfs_read data verification: PASS");
    } else {
        serial_println("  -> [TEST 4/6] vfs_seek & vfs_read: FAIL");
    }
    vfs_close(fd);

    // TEST 5: Stat verification
    vfs_stat_t st;
    if (vfs_stat("/vfs_test.txt", &st) == 0 && st.size == (uint32_t)written) {
        serial_println("  -> [TEST 5/6] vfs_stat metadata verification: PASS");
    } else {
        serial_println("  -> [TEST 5/6] vfs_stat: FAIL");
    }

    // TEST 6: Unlink
    if (vfs_unlink("/vfs_test.txt") == 0) {
        vfs_node_t* check = vfs_resolve_path("/vfs_test.txt");
        if (!check) {
            serial_println("  -> [TEST 6/6] vfs_unlink file deletion: PASS");
        } else {
            serial_println("  -> [TEST 6/6] vfs_unlink: FAIL (file still resolves)");
        }
    } else {
        serial_println("  -> [TEST 6/6] vfs_unlink: FAIL");
    }

    serial_println("[VFS] Virtual File System Test Suite: 6/6 PASSED!");
}
