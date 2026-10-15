#ifndef PROC_MGR_H
#define PROC_MGR_H

#include <stdint.h>
#include <stddef.h>
#include "posix_time.h"

#define PROC_TABLE_MAX 64

#ifndef PROC_STATE_UNUSED
#define PROC_STATE_UNUSED   0
#define PROC_STATE_READY    1
#define PROC_STATE_RUNNING  2
#endif
#define PROC_STATE_SLEEPING 3
#define PROC_STATE_STOPPED  4
#define PROC_STATE_ZOMBIE   5

// Standard POSIX signals
#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGKILL   9
#define SIGUSR1   10
#define SIGUSR2   12
#define SIGTERM   15
#define SIGCONT   18
#define SIGSTOP   19

typedef int32_t pid_t;

typedef struct {
    pid_t    pid;
    pid_t    ppid;
    char     name[32];
    char     cmdline[64];
    int      state;
    int      priority; // -20 to 19 (nice)
    uint32_t cpu_ticks;
    uint32_t cpu_usage_pct;
    uint32_t mem_usage_kb;
    time_t   start_time;
    int      exit_code;
    uint32_t signal_pending;
    int      is_daemon;
    int      active;
} pcb_t;

void     proc_mgr_init(void);
pid_t    proc_spawn(const char* name, const char* cmdline, int is_daemon);
pid_t    proc_fork(pid_t parent_pid);
int      proc_exit(pid_t pid, int exit_code);
int      proc_kill(pid_t pid, int sig);
int      proc_waitpid(pid_t pid, int* status, int options);
pcb_t*   proc_get(pid_t pid);
int      proc_get_all(pcb_t* out_list, int max_count);
int      proc_get_active_count(void);
void     proc_tick(void);

// Verification test suite
void     proc_mgr_run_tests(void);

#endif
