#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <sys/types.h>

#define WNOHANG     1
#define WUNTRACED   2

#define WIFEXITED(status)    (((status) & 0x7f) == 0)
#define WEXITSTATUS(status)  (((status) & 0xff00) >> 8)
#define WIFSIGNALED(status)  (((status) & 0x7f) != 0 && ((status) & 0x7f) != 0x7f)
#define WTERMSIG(status)     ((status) & 0x7f)
#define WIFSTOPPED(status)   (((status) & 0xff) == 0x7f)
#define WSTOPSIG(status)     WEXITSTATUS(status)

pid_t wait(int* wstatus);
pid_t waitpid(pid_t pid, int* wstatus, int options);

#endif // _SYS_WAIT_H
