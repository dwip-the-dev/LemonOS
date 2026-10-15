#ifndef SYS_STAT_H
#define SYS_STAT_H

#include <stdint.h>

typedef uint32_t mode_t;

struct stat {
    uint32_t st_dev;
    uint32_t st_ino;
    mode_t   st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t st_rdev;
    uint32_t st_size;
    uint32_t st_atime;
    uint32_t st_mtime;
    uint32_t st_ctime;
};

#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFREG  0100000
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)

int mkdir(const char *pathname, mode_t mode);
int stat(const char *pathname, struct stat *statbuf);
int fstat(int fd, struct stat *statbuf);

#endif
