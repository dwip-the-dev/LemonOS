#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

// VFS Node Flags / Types
#define VFS_FILE         0x01
#define VFS_DIRECTORY    0x02
#define VFS_CHARDEVICE   0x03
#define VFS_BLOCKDEVICE  0x04
#define VFS_PIPE         0x05
#define VFS_SYMLINK      0x06
#define VFS_MOUNTPOINT   0x08

// Open Flags
#define O_RDONLY         0x0000
#define O_WRONLY         0x0001
#define O_RDWR           0x0002
#define O_CREAT          0x0040
#define O_EXCL           0x0080
#define O_TRUNC          0x0200
#define O_APPEND         0x0400

// Seek Constants
#define SEEK_SET         0
#define SEEK_CUR         1
#define SEEK_END         2

#define VFS_MAX_PATH     256
#define VFS_NAME_MAX     64
#define VFS_MAX_MOUNTS   16
#define VFS_MAX_FDS      64

struct vfs_node;
typedef struct vfs_node vfs_node_t;

// Directory Entry
typedef struct {
    char     name[VFS_NAME_MAX];
    uint32_t inode;
    uint32_t type;
} vfs_dirent_t;

// Stat metadata
typedef struct {
    uint32_t inode;
    uint32_t size;
    uint32_t flags;
    uint32_t permissions;
    uint32_t uid;
    uint32_t gid;
    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;
} vfs_stat_t;

// Node operations function pointers
typedef int           (*vfs_read_fn)(vfs_node_t* node, uint32_t offset, uint32_t size, void* buf);
typedef int           (*vfs_write_fn)(vfs_node_t* node, uint32_t offset, uint32_t size, const void* buf);
typedef int           (*vfs_open_fn)(vfs_node_t* node, int flags);
typedef int           (*vfs_close_fn)(vfs_node_t* node);
typedef int           (*vfs_readdir_fn)(vfs_node_t* node, uint32_t index, vfs_dirent_t* out_dirent);
typedef vfs_node_t*   (*vfs_finddir_fn)(vfs_node_t* node, const char* name);
typedef vfs_node_t*   (*vfs_create_fn)(vfs_node_t* parent, const char* name, uint16_t mode);
typedef vfs_node_t*   (*vfs_mkdir_fn)(vfs_node_t* parent, const char* name, uint16_t mode);
typedef int           (*vfs_unlink_fn)(vfs_node_t* parent, const char* name);
typedef int           (*vfs_stat_fn)(vfs_node_t* node, vfs_stat_t* out_stat);

// Virtual File System Node
struct vfs_node {
    char            name[VFS_NAME_MAX];
    uint32_t        flags;
    uint32_t        inode;
    uint32_t        size;
    uint32_t        permissions;
    uint32_t        uid;
    uint32_t        gid;
    uint32_t        atime;
    uint32_t        mtime;
    uint32_t        ctime;

    // Callbacks
    vfs_read_fn     read;
    vfs_write_fn    write;
    vfs_open_fn     open;
    vfs_close_fn    close;
    vfs_readdir_fn  readdir;
    vfs_finddir_fn  finddir;
    vfs_create_fn   create;
    vfs_mkdir_fn    mkdir;
    vfs_unlink_fn   unlink;
    vfs_stat_fn     stat;

    void*           ptr;          // Driver context pointer
    vfs_node_t*     mount_point;  // Mounted node if VFS_MOUNTPOINT
};

// File descriptor entry
typedef struct {
    vfs_node_t* node;
    uint32_t    offset;
    int         flags;
    int         in_use;
} vfs_file_t;

// VFS Mount entry
typedef struct {
    char        path[VFS_MAX_PATH];
    vfs_node_t* root_node;
    int         in_use;
} vfs_mount_t;

// Core VFS Management API
void         vfs_init(void);
int          vfs_mount(const char* path, vfs_node_t* root_node);
int          vfs_unmount(const char* path);
vfs_node_t*  vfs_resolve_path(const char* path);
vfs_node_t*  vfs_get_root(void);

// POSIX-style File Operations API
int          vfs_open(const char* path, int flags);
int          vfs_close(int fd);
int          vfs_read(int fd, void* buffer, uint32_t size);
int          vfs_write(int fd, const void* buffer, uint32_t size);
int          vfs_seek(int fd, int32_t offset, int whence);
int          vfs_stat(const char* path, vfs_stat_t* out_stat);
int          vfs_mkdir(const char* path, uint16_t mode);
int          vfs_unlink(const char* path);
int          vfs_readdir(const char* path, uint32_t index, vfs_dirent_t* out_dirent);
int          vfs_allocate_fd(vfs_node_t* node, int flags);
vfs_node_t*  vfs_get_node(int fd);
vfs_file_t*  vfs_get_file(int fd);
int          vfs_dup(int oldfd);
int          vfs_dup2(int oldfd, int newfd);
int          vfs_dup_min(int oldfd, int min_fd);
int          vfs_tell(int fd);
int          vfs_lseek(int fd, int32_t offset, int whence);

// VFS Information & Verification Suite
void         vfs_list_mounts(void);
void         vfs_run_tests(void);

#endif
