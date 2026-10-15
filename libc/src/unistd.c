#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include "syscall_nums.h"

pid_t fork(void) {
    return (pid_t)__syscall_ret(__syscall0(SYS_FORK));
}

int execve(const char* filename, char* const argv[], char* const envp[]) {
    return __syscall_ret(__syscall3(SYS_EXECVE, (unsigned int)filename, (unsigned int)argv, (unsigned int)envp));
}

int execv(const char* path, char* const argv[]) {
    return execve(path, argv, environ);
}

int execl(const char* path, const char* arg, ...) {
    va_list ap;
    va_start(ap, arg);

    int count = 1;
    while (va_arg(ap, const char*) != NULL) {
        count++;
    }
    va_end(ap);

    char** argv = (char**)malloc(sizeof(char*) * (count + 1));
    if (!argv) return -1;

    argv[0] = (char*)arg;
    va_start(ap, arg);
    for (int i = 1; i < count; i++) {
        argv[i] = va_arg(ap, char*);
    }
    argv[count] = NULL;
    va_end(ap);

    int ret = execv(path, argv);
    free(argv);
    return ret;
}

void _exit(int status) {
    __syscall1(SYS_EXIT, (unsigned int)status);
    while (1) { asm volatile("hlt"); }
}

ssize_t read(int fd, void* buf, size_t count) {
    return (ssize_t)__syscall_ret(__syscall3(SYS_READ, (unsigned int)fd, (unsigned int)buf, (unsigned int)count));
}

ssize_t write(int fd, const void* buf, size_t count) {
    return (ssize_t)__syscall_ret(__syscall3(SYS_WRITE, (unsigned int)fd, (unsigned int)buf, (unsigned int)count));
}

int close(int fd) {
    return __syscall_ret(__syscall1(SYS_CLOSE, (unsigned int)fd));
}

off_t lseek(int fd, off_t offset, int whence) {
    return (off_t)__syscall_ret(__syscall3(SYS_LSEEK, (unsigned int)fd, (unsigned int)offset, (unsigned int)whence));
}

int dup(int oldfd) {
    return __syscall_ret(__syscall1(SYS_DUP, (unsigned int)oldfd));
}

int dup2(int oldfd, int newfd) {
    return __syscall_ret(__syscall2(SYS_DUP2, (unsigned int)oldfd, (unsigned int)newfd));
}

int pipe(int pipefd[2]) {
    return __syscall_ret(__syscall1(SYS_PIPE, (unsigned int)pipefd));
}

int isatty(int fd) {
    struct termios t;
    return (tcgetattr(fd, &t) == 0) ? 1 : 0;
}

int unlink(const char* pathname) {
    return __syscall_ret(__syscall1(SYS_UNLINK, (unsigned int)pathname));
}

int rmdir(const char* pathname) {
    return __syscall_ret(__syscall1(SYS_RMDIR, (unsigned int)pathname));
}

int chdir(const char* path) {
    return __syscall_ret(__syscall1(SYS_CHDIR, (unsigned int)path));
}

char* getcwd(char* buf, size_t size) {
    int ret = __syscall2(SYS_GETCWD, (unsigned int)buf, (unsigned int)size);
    if (ret < 0) {
        errno = -ret;
        return NULL;
    }
    return buf;
}

int access(const char* pathname, int mode) {
    return __syscall_ret(__syscall2(SYS_ACCESS, (unsigned int)pathname, (unsigned int)mode));
}

void sync(void) {
    __syscall0(SYS_SYNC);
}

pid_t getpid(void) {
    return (pid_t)__syscall0(SYS_GETPID);
}

pid_t getppid(void) {
    return (pid_t)__syscall0(SYS_GETPPID);
}

uid_t getuid(void) {
    return (uid_t)__syscall0(SYS_GETUID);
}

uid_t geteuid(void) {
    return (uid_t)__syscall0(SYS_GETEUID);
}

gid_t getgid(void) {
    return (gid_t)__syscall0(SYS_GETGID);
}

gid_t getegid(void) {
    return (gid_t)__syscall0(SYS_GETEGID);
}

int setuid(uid_t uid) {
    return __syscall_ret(__syscall1(SYS_SETUID, (unsigned int)uid));
}

int setgid(gid_t gid) {
    return __syscall_ret(__syscall1(SYS_SETGID, (unsigned int)gid));
}

unsigned int sleep(unsigned int seconds) {
    // 69 is SYS_SLEEP_MS or use nanosleep
    __syscall1(69, seconds * 1000);
    return 0;
}

int usleep(useconds_t usec) {
    unsigned int ms = usec / 1000;
    if (ms == 0 && usec > 0) ms = 1;
    __syscall1(69, ms);
    return 0;
}

int pause(void) {
    return __syscall_ret(__syscall0(SYS_PAUSE));
}

unsigned int alarm(unsigned int seconds) {
    return (unsigned int)__syscall1(SYS_ALARM, seconds);
}

void* sbrk(intptr_t increment) {
    int ret = __syscall1(SYS_BRK, (unsigned int)increment);
    if (ret < 0) {
        errno = ENOMEM;
        return (void*)-1;
    }
    return (void*)ret;
}

int brk(void* addr) {
    (void)addr;
    return 0;
}

// open / creat
int open(const char* pathname, int flags, ...) {
    return __syscall_ret(__syscall2(SYS_OPEN, (unsigned int)pathname, (unsigned int)flags));
}

int creat(const char* pathname, mode_t mode) {
    return __syscall_ret(__syscall2(SYS_CREAT, (unsigned int)pathname, (unsigned int)mode));
}

int fcntl(int fd, int cmd, ...) {
    va_list ap;
    va_start(ap, cmd);
    unsigned int arg = va_arg(ap, unsigned int);
    va_end(ap);
    return __syscall_ret(__syscall3(SYS_FCNTL, (unsigned int)fd, (unsigned int)cmd, arg));
}
