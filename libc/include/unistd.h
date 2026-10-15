#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

extern char** environ;

// Process Management
pid_t fork(void);
int   execv(const char* path, char* const argv[]);
int   execve(const char* filename, char* const argv[], char* const envp[]);
int   execl(const char* path, const char* arg, ...);
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int   setuid(uid_t uid);
int   setgid(gid_t gid);

// I/O Operations
ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
int     close(int fd);
off_t   lseek(int fd, off_t offset, int whence);
int     dup(int oldfd);
int     dup2(int oldfd, int newfd);
int     pipe(int pipefd[2]);
int     isatty(int fd);

// File & Directory Operations
int   unlink(const char* pathname);
int   rmdir(const char* pathname);
int   chdir(const char* path);
char* getcwd(char* buf, size_t size);
int   access(const char* pathname, int mode);
void  sync(void);

// Execution control / Sleep
unsigned int sleep(unsigned int seconds);
int          usleep(useconds_t usec);
int          pause(void);
unsigned int alarm(unsigned int seconds);

// Heap management
void* sbrk(intptr_t increment);
int   brk(void* addr);

#endif // _UNISTD_H
