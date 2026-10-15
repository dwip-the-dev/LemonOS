/**
 * @file signal.h
 * @brief POSIX Signals Subsystem for LemonOS
 * @author Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>
 *
 * Implements complete POSIX signal definitions, signal sets, sigaction handlers,
 * per-process pending/blocked masks, and signal delivery logic.
 */

#ifndef SIGNAL_H
#define SIGNAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Standard POSIX signal numbers
#ifndef SIGHUP
#define SIGHUP    1
#endif
#ifndef SIGINT
#define SIGINT    2
#endif
#ifndef SIGQUIT
#define SIGQUIT   3
#endif
#ifndef SIGILL
#define SIGILL    4
#endif
#ifndef SIGTRAP
#define SIGTRAP   5
#endif
#ifndef SIGABRT
#define SIGABRT   6
#endif
#ifndef SIGBUS
#define SIGBUS    7
#endif
#ifndef SIGFPE
#define SIGFPE    8
#endif
#ifndef SIGKILL
#define SIGKILL   9
#endif
#ifndef SIGUSR1
#define SIGUSR1   10
#endif
#ifndef SIGSEGV
#define SIGSEGV   11
#endif
#ifndef SIGUSR2
#define SIGUSR2   12
#endif
#ifndef SIGPIPE
#define SIGPIPE   13
#endif
#ifndef SIGALRM
#define SIGALRM   14
#endif
#ifndef SIGTERM
#define SIGTERM   15
#endif
#ifndef SIGCHLD
#define SIGCHLD   17
#endif
#ifndef SIGCONT
#define SIGCONT   18
#endif
#ifndef SIGSTOP
#define SIGSTOP   19
#endif
#ifndef SIGTSTP
#define SIGTSTP   20
#endif
#ifndef SIGTTIN
#define SIGTTIN   21
#endif
#ifndef SIGTTOU
#define SIGTTOU   22
#endif
#ifndef SIGURG
#define SIGURG    23
#endif
#ifndef SIGXCPU
#define SIGXCPU   24
#endif
#ifndef SIGXFSZ
#define SIGXFSZ   25
#endif
#ifndef SIGVTALRM
#define SIGVTALRM 26
#endif
#ifndef SIGPROF
#define SIGPROF   27
#endif
#ifndef SIGWINCH
#define SIGWINCH  28
#endif
#ifndef SIGIO
#define SIGIO     29
#endif
#ifndef SIGPWR
#define SIGPWR    30
#endif
#ifndef SIGSYS
#define SIGSYS    31
#endif

#define NSIG      32

// Special signal handlers
typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

// Signal sets (32 signals represented by bitmask)
typedef uint32_t sigset_t;

// Signal blocking flags
#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

// sigaction flags
#define SA_RESTART    0x10000000
#define SA_NODEFER    0x40000000
#define SA_RESETHAND  0x80000000

// Signal action structure
struct sigaction {
    sighandler_t sa_handler;
    sigset_t     sa_mask;
    int          sa_flags;
};

// Core POSIX Signal API
void         signal_init(void);
int          signal_send(int pid, int sig);
sighandler_t signal_set_handler(int pid, int sig, sighandler_t handler);
int          signal_action(int pid, int sig, const struct sigaction* act, struct sigaction* oldact);
int          signal_procmask(int pid, int how, const sigset_t* set, sigset_t* oldset);
int          signal_pending(int pid, sigset_t* set);
int          signal_deliver_pending(int pid);
int          signal_has_pending(int pid);
const char*  signal_name(int sig);

// Reset process signal state (used on fork/exec)
void         signal_proc_init(int pid);
void         signal_proc_cleanup(int pid);

// Automated Verification Test Suite
void         signal_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif // SIGNAL_H
