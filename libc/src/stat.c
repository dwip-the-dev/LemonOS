#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include "syscall_nums.h"

int stat(const char* pathname, struct stat* statbuf) {
    return __syscall_ret(__syscall2(SYS_STAT, (unsigned int)pathname, (unsigned int)statbuf));
}

int fstat(int fd, struct stat* statbuf) {
    return __syscall_ret(__syscall2(SYS_FSTAT, (unsigned int)fd, (unsigned int)statbuf));
}

int lstat(const char* pathname, struct stat* statbuf) {
    return stat(pathname, statbuf);
}

int mkdir(const char* pathname, mode_t mode) {
    return __syscall_ret(__syscall2(SYS_MKDIR, (unsigned int)pathname, (unsigned int)mode));
}

int chmod(const char* pathname, mode_t mode) {
    return __syscall_ret(__syscall2(SYS_CHMOD, (unsigned int)pathname, (unsigned int)mode));
}

int fchmod(int fd, mode_t mode) {
    (void)fd; (void)mode;
    return 0;
}

pid_t wait(int* wstatus) {
    return waitpid(-1, wstatus, 0);
}

pid_t waitpid(pid_t pid, int* wstatus, int options) {
    return (pid_t)__syscall_ret(__syscall3(SYS_WAITPID, (unsigned int)pid, (unsigned int)wstatus, (unsigned int)options));
}
