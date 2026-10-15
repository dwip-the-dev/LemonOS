#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "syscall_nums.h"

DIR* opendir(const char* name) {
    if (!name) {
        errno = ENOENT;
        return NULL;
    }

    int fd = open(name, O_RDONLY | O_DIRECTORY);
    if (fd < 0) return NULL;

    DIR* dirp = (DIR*)malloc(sizeof(DIR));
    if (!dirp) {
        close(fd);
        errno = ENOMEM;
        return NULL;
    }

    dirp->fd = fd;
    dirp->index = 0;
    return dirp;
}

struct dirent* readdir(DIR* dirp) {
    if (!dirp) {
        errno = EBADF;
        return NULL;
    }

    int ret = __syscall3(SYS_GETDENTS, (unsigned int)dirp->fd, (unsigned int)dirp->index, (unsigned int)&dirp->ent);
    if (ret <= 0) {
        if (ret < 0) errno = -ret;
        return NULL;
    }

    dirp->index++;
    return &dirp->ent;
}

int closedir(DIR* dirp) {
    if (!dirp) {
        errno = EBADF;
        return -1;
    }
    int res = close(dirp->fd);
    free(dirp);
    return res;
}

void rewinddir(DIR* dirp) {
    if (dirp) dirp->index = 0;
}
