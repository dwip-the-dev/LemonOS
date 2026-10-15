#include "proc_mgr.h"
#include "string.h"
#include "serial.h"

static pcb_t proc_table[PROC_TABLE_MAX];
static pid_t next_pid = 1;
static uint32_t total_system_ticks = 0;

void proc_mgr_init(void) {
    memset(proc_table, 0, sizeof(proc_table));
    next_pid = 1;
    total_system_ticks = 0;

    // 1. Initialize PID 1: systemd / init process
    pcb_t* init_proc = &proc_table[0];
    init_proc->pid = 1;
    init_proc->ppid = 0;
    strncpy(init_proc->name, "systemd", sizeof(init_proc->name) - 1);
    strncpy(init_proc->cmdline, "/sbin/init splash", sizeof(init_proc->cmdline) - 1);
    init_proc->state = PROC_STATE_SLEEPING;
    init_proc->priority = 0;
    init_proc->cpu_ticks = 12;
    init_proc->cpu_usage_pct = 1;
    init_proc->mem_usage_kb = 4096;
    init_proc->start_time = posix_time(NULL);
    init_proc->is_daemon = 1;
    init_proc->active = 1;

    // 2. Initialize PID 2: kthreadd
    pcb_t* kthread = &proc_table[1];
    kthread->pid = 2;
    kthread->ppid = 0;
    strncpy(kthread->name, "kthreadd", sizeof(kthread->name) - 1);
    strncpy(kthread->cmdline, "[kthreadd]", sizeof(kthread->cmdline) - 1);
    kthread->state = PROC_STATE_SLEEPING;
    kthread->priority = -5;
    kthread->cpu_ticks = 8;
    kthread->cpu_usage_pct = 1;
    kthread->mem_usage_kb = 2048;
    kthread->start_time = posix_time(NULL);
    kthread->is_daemon = 1;
    kthread->active = 1;

    // 3. Initialize PID 3: LemonDE Compositor
    pcb_t* de_proc = &proc_table[2];
    de_proc->pid = 3;
    de_proc->ppid = 1;
    strncpy(de_proc->name, "lemon-compositor", sizeof(de_proc->name) - 1);
    strncpy(de_proc->cmdline, "/bin/lemon-desktop", sizeof(de_proc->cmdline) - 1);
    de_proc->state = PROC_STATE_RUNNING;
    de_proc->priority = 0;
    de_proc->cpu_ticks = 45;
    de_proc->cpu_usage_pct = 5;
    de_proc->mem_usage_kb = 18432;
    de_proc->start_time = posix_time(NULL);
    de_proc->is_daemon = 0;
    de_proc->active = 1;

    next_pid = 4;
    serial_println("[PROC] Process manager & Init system initialized (PID 1: systemd active).");
}

pid_t proc_spawn(const char* name, const char* cmdline, int is_daemon) {
    if (!name || name[0] == '\0') return -1;

    for (int i = 0; i < PROC_TABLE_MAX; i++) {
        if (!proc_table[i].active) {
            pcb_t* p = &proc_table[i];
            memset(p, 0, sizeof(pcb_t));
            p->pid = next_pid++;
            p->ppid = 1; // Default parent PID 1
            strncpy(p->name, name, sizeof(p->name) - 1);
            p->name[sizeof(p->name) - 1] = '\0';
            if (cmdline && cmdline[0] != '\0') {
                strncpy(p->cmdline, cmdline, sizeof(p->cmdline) - 1);
                p->cmdline[sizeof(p->cmdline) - 1] = '\0';
            } else {
                strncpy(p->cmdline, name, sizeof(p->cmdline) - 1);
            }
            p->state = PROC_STATE_READY;
            p->priority = 0;
            p->cpu_ticks = 1;
            p->cpu_usage_pct = 1;
            p->mem_usage_kb = 2048;
            p->start_time = posix_time(NULL);
            p->is_daemon = is_daemon;
            p->active = 1;
            return p->pid;
        }
    }
    return -1; // Process table full
}

pid_t proc_fork(pid_t parent_pid) {
    pcb_t* parent = proc_get(parent_pid);
    if (!parent) return -1;

    for (int i = 0; i < PROC_TABLE_MAX; i++) {
        if (!proc_table[i].active) {
            pcb_t* child = &proc_table[i];
            *child = *parent; // Copy state
            child->pid = next_pid++;
            child->ppid = parent_pid;
            child->cpu_ticks = 0;
            child->cpu_usage_pct = 0;
            child->start_time = posix_time(NULL);
            child->state = PROC_STATE_READY;
            child->active = 1;
            return child->pid;
        }
    }
    return -1;
}

int proc_exit(pid_t pid, int exit_code) {
    if (pid == 1) {
        serial_println("[PROC] WARNING: Attempted to kill init (PID 1) ignored!");
        return -1;
    }

    pcb_t* p = proc_get(pid);
    if (!p) return -1;

    p->state = PROC_STATE_ZOMBIE;
    p->exit_code = exit_code;

    // Reparent any child processes of this exiting process to PID 1 (init)
    for (int i = 0; i < PROC_TABLE_MAX; i++) {
        if (proc_table[i].active && proc_table[i].ppid == pid) {
            proc_table[i].ppid = 1;
        }
    }
    return 0;
}

int proc_kill(pid_t pid, int sig) {
    pcb_t* p = proc_get(pid);
    if (!p) return -1;

    switch (sig) {
        case SIGKILL:
            return proc_exit(pid, 128 + SIGKILL);

        case SIGTERM:
        case SIGINT:
        case SIGHUP:
            return proc_exit(pid, 128 + sig);

        case SIGSTOP:
            p->state = PROC_STATE_STOPPED;
            return 0;

        case SIGCONT:
            if (p->state == PROC_STATE_STOPPED) {
                p->state = PROC_STATE_READY;
            }
            return 0;

        default:
            p->signal_pending |= (1 << sig);
            return 0;
    }
}

int proc_waitpid(pid_t pid, int* status, int options) {
    (void)options;
    for (int i = 0; i < PROC_TABLE_MAX; i++) {
        if (proc_table[i].active && (pid == -1 || proc_table[i].pid == pid)) {
            if (proc_table[i].state == PROC_STATE_ZOMBIE) {
                if (status) *status = proc_table[i].exit_code;
                pid_t reaped_pid = proc_table[i].pid;
                proc_table[i].active = 0;
                return reaped_pid;
            }
        }
    }
    return 0;
}

pcb_t* proc_get(pid_t pid) {
    for (int i = 0; i < PROC_TABLE_MAX; i++) {
        if (proc_table[i].active && proc_table[i].pid == pid) {
            return &proc_table[i];
        }
    }
    return NULL;
}

int proc_get_all(pcb_t* out_list, int max_count) {
    if (!out_list || max_count <= 0) return 0;
    int count = 0;
    for (int i = 0; i < PROC_TABLE_MAX && count < max_count; i++) {
        if (proc_table[i].active) {
            out_list[count++] = proc_table[i];
        }
    }
    return count;
}

int proc_get_active_count(void) {
    int count = 0;
    for (int i = 0; i < PROC_TABLE_MAX; i++) {
        if (proc_table[i].active && proc_table[i].state != PROC_STATE_ZOMBIE) {
            count++;
        }
    }
    return count;
}

void proc_tick(void) {
    total_system_ticks++;
    // Periodically update process CPU time calculations
    for (int i = 0; i < PROC_TABLE_MAX; i++) {
        if (proc_table[i].active && proc_table[i].state == PROC_STATE_RUNNING) {
            proc_table[i].cpu_ticks++;
        }
    }
}

void proc_mgr_run_tests(void) {
    serial_println("[PROC] Running Process Manager Verification Tests...");

    // 1. Verify PID 1
    pcb_t* p1 = proc_get(1);
    if (p1 && strcmp(p1->name, "systemd") == 0) {
        serial_println("  -> [TEST 1/3] PID 1 systemd init process: PASS");
    } else {
        serial_println("  -> [TEST 1/3] PID 1 init process: FAIL");
    }

    // 2. Spawn and Fork
    pid_t spawned = proc_spawn("test_worker", "/bin/worker --job 1", 0);
    if (spawned > 0) {
        pcb_t* sp = proc_get(spawned);
        if (sp && sp->ppid == 1 && strcmp(sp->name, "test_worker") == 0) {
            serial_println("  -> [TEST 2/3] proc_spawn: PASS");
        } else {
            serial_println("  -> [TEST 2/3] proc_spawn: FAIL");
        }
    } else {
        serial_println("  -> [TEST 2/3] proc_spawn: FAIL");
    }

    // 3. Kill and Reap
    if (spawned > 0) {
        proc_kill(spawned, SIGKILL);
        pcb_t* killed = proc_get(spawned);
        if (killed && killed->state == PROC_STATE_ZOMBIE) {
            int st = 0;
            pid_t reaped = proc_waitpid(spawned, &st, 0);
            if (reaped == spawned && proc_get(spawned) == NULL) {
                serial_println("  -> [TEST 3/3] proc_kill & proc_waitpid zombie reaping: PASS");
            } else {
                serial_println("  -> [TEST 3/3] proc_waitpid: FAIL");
            }
        } else {
            serial_println("  -> [TEST 3/3] proc_kill: FAIL");
        }
    }
    serial_println("[PROC] Process manager verified.");
}
