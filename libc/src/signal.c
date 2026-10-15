#include <signal.h>
#include <errno.h>
#include "syscall_nums.h"

int kill(pid_t pid, int sig) {
    return __syscall_ret(__syscall2(SYS_KILL, (unsigned int)pid, (unsigned int)sig));
}

sighandler_t signal(int signum, sighandler_t handler) {
    int ret = __syscall2(SYS_SIGNAL, (unsigned int)signum, (unsigned int)handler);
    if (ret < 0) {
        errno = -ret;
        return SIG_ERR;
    }
    return (sighandler_t)ret;
}

int sigaction(int signum, const struct sigaction* act, struct sigaction* oldact) {
    return __syscall_ret(__syscall3(SYS_SIGACTION, (unsigned int)signum, (unsigned int)act, (unsigned int)oldact));
}

int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
    return __syscall_ret(__syscall3(SYS_SIGPROCMASK, (unsigned int)how, (unsigned int)set, (unsigned int)oldset));
}

int sigemptyset(sigset_t* set) {
    if (!set) return -1;
    *set = 0;
    return 0;
}

int sigfillset(sigset_t* set) {
    if (!set) return -1;
    *set = 0xFFFFFFFF;
    return 0;
}

int sigaddset(sigset_t* set, int signum) {
    if (!set || signum < 1 || signum > 32) return -1;
    *set |= (1U << (signum - 1));
    return 0;
}

int sigdelset(sigset_t* set, int signum) {
    if (!set || signum < 1 || signum > 32) return -1;
    *set &= ~(1U << (signum - 1));
    return 0;
}

int sigismember(const sigset_t* set, int signum) {
    if (!set || signum < 1 || signum > 32) return -1;
    return (*set & (1U << (signum - 1))) ? 1 : 0;
}
