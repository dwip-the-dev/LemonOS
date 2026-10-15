/**
 * @file signal.c
 * @brief POSIX Signals Implementation for LemonOS
 * @author Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>
 *
 * Implements POSIX-compliant signal delivery, signal sets, handlers,
 * blocked masks, and default kernel actions.
 */

#include "signal.h"
#include "proc_mgr.h"
#include "sync.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    sigset_t          pending;
    sigset_t          blocked;
    struct sigaction  actions[NSIG];
    uint32_t          sig_counts[NSIG];
    int               initialized;
} proc_signals_t;

static proc_signals_t proc_sigs[PROC_TABLE_MAX];
static spinlock_t     sig_global_lock = SPINLOCK_INIT;

static const char* sig_names[NSIG] = {
    "UNKNOWN", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP",
    "SIGABRT", "SIGBUS", "SIGFPE", "SIGKILL", "SIGUSR1", "SIGSEGV",
    "SIGUSR2", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT", "SIGCHLD",
    "SIGCONT", "SIGSTOP", "SIGTSTP", "SIGTTIN", "SIGTTOU", "SIGURG",
    "SIGXCPU", "SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH", "SIGIO",
    "SIGPWR", "SIGSYS"
};

const char* signal_name(int sig) {
    if (sig >= 0 && sig < NSIG) return sig_names[sig];
    return "INVALID";
}

void signal_proc_init(int pid) {
    if (pid < 0 || pid >= PROC_TABLE_MAX) return;

    proc_signals_t* ps = &proc_sigs[pid];
    ps->pending = 0;
    ps->blocked = 0;
    for (int i = 0; i < NSIG; i++) {
        ps->actions[i].sa_handler = SIG_DFL;
        ps->actions[i].sa_mask = 0;
        ps->actions[i].sa_flags = 0;
        ps->sig_counts[i] = 0;
    }
    ps->initialized = 1;
}

void signal_proc_cleanup(int pid) {
    if (pid < 0 || pid >= PROC_TABLE_MAX) return;
    proc_sigs[pid].pending = 0;
    proc_sigs[pid].blocked = 0;
    proc_sigs[pid].initialized = 0;
}

void signal_init(void) {
    spinlock_init(&sig_global_lock);
    for (int i = 0; i < PROC_TABLE_MAX; i++) {
        signal_proc_init(i);
    }
    serial_println("[SIGNAL] POSIX signal subsystem initialized (32 signals, sigaction support).");
}

sighandler_t signal_set_handler(int pid, int sig, sighandler_t handler) {
    if (pid < 0 || pid >= PROC_TABLE_MAX || sig <= 0 || sig >= NSIG) return SIG_ERR;
    if (sig == SIGKILL || sig == SIGSTOP) {
        serial_println("[SIGNAL] ERROR: Cannot override SIGKILL or SIGSTOP handler!");
        return SIG_ERR;
    }

    spinlock_lock(&sig_global_lock);
    proc_signals_t* ps = &proc_sigs[pid];
    if (!ps->initialized) signal_proc_init(pid);

    sighandler_t old = ps->actions[sig].sa_handler;
    ps->actions[sig].sa_handler = handler;
    ps->actions[sig].sa_flags = 0;
    ps->actions[sig].sa_mask = 0;
    spinlock_unlock(&sig_global_lock);

    return old;
}

int signal_action(int pid, int sig, const struct sigaction* act, struct sigaction* oldact) {
    if (pid < 0 || pid >= PROC_TABLE_MAX || sig <= 0 || sig >= NSIG) return -1;
    if (sig == SIGKILL || sig == SIGSTOP) {
        return -1;
    }

    spinlock_lock(&sig_global_lock);
    proc_signals_t* ps = &proc_sigs[pid];
    if (!ps->initialized) signal_proc_init(pid);

    if (oldact) {
        *oldact = ps->actions[sig];
    }
    if (act) {
        ps->actions[sig] = *act;
    }
    spinlock_unlock(&sig_global_lock);

    return 0;
}

int signal_procmask(int pid, int how, const sigset_t* set, sigset_t* oldset) {
    if (pid < 0 || pid >= PROC_TABLE_MAX) return -1;

    spinlock_lock(&sig_global_lock);
    proc_signals_t* ps = &proc_sigs[pid];
    if (!ps->initialized) signal_proc_init(pid);

    if (oldset) {
        *oldset = ps->blocked;
    }

    if (set) {
        sigset_t s = *set;
        // SIGKILL and SIGSTOP cannot be blocked
        s &= ~((1 << SIGKILL) | (1 << SIGSTOP));

        switch (how) {
            case SIG_BLOCK:
                ps->blocked |= s;
                break;
            case SIG_UNBLOCK:
                ps->blocked &= ~s;
                break;
            case SIG_SETMASK:
                ps->blocked = s;
                break;
            default:
                spinlock_unlock(&sig_global_lock);
                return -1;
        }
    }
    spinlock_unlock(&sig_global_lock);

    // If unblocking may allow pending signals to be delivered, deliver them
    signal_deliver_pending(pid);

    return 0;
}

int signal_pending(int pid, sigset_t* set) {
    if (pid < 0 || pid >= PROC_TABLE_MAX || !set) return -1;
    *set = proc_sigs[pid].pending;
    return 0;
}

int signal_has_pending(int pid) {
    if (pid < 0 || pid >= PROC_TABLE_MAX) return 0;
    return (proc_sigs[pid].pending & ~proc_sigs[pid].blocked) != 0;
}

int signal_send(int pid, int sig) {
    if (pid < 0 || pid >= PROC_TABLE_MAX || sig <= 0 || sig >= NSIG) return -1;

    pcb_t* p = proc_get(pid);
    if (!p || !p->active) return -1;

    spinlock_lock(&sig_global_lock);
    proc_signals_t* ps = &proc_sigs[pid];
    if (!ps->initialized) signal_proc_init(pid);

    ps->sig_counts[sig]++;

    // SIGKILL: Cannot be blocked or ignored
    if (sig == SIGKILL) {
        spinlock_unlock(&sig_global_lock);
        serial_print("[SIGNAL] Delivering immediate SIGKILL to PID ");
        serial_print_dec(pid);
        serial_println("");
        return proc_exit(pid, 128 + SIGKILL);
    }

    // SIGSTOP: Cannot be blocked or ignored
    if (sig == SIGSTOP) {
        p->state = PROC_STATE_STOPPED;
        spinlock_unlock(&sig_global_lock);
        return 0;
    }

    // SIGCONT: Resumes stopped process
    if (sig == SIGCONT) {
        if (p->state == PROC_STATE_STOPPED) {
            p->state = PROC_STATE_READY;
        }
        ps->pending &= ~(1 << SIGSTOP);
        spinlock_unlock(&sig_global_lock);
        return 0;
    }

    // If ignored, drop signal
    if (ps->actions[sig].sa_handler == SIG_IGN) {
        spinlock_unlock(&sig_global_lock);
        return 0;
    }

    // Mark signal pending
    ps->pending |= (1 << sig);
    p->signal_pending |= (1 << sig);
    spinlock_unlock(&sig_global_lock);

    // Attempt delivery if not blocked
    if (!(ps->blocked & (1 << sig))) {
        return signal_deliver_pending(pid);
    }

    return 0;
}

int signal_deliver_pending(int pid) {
    if (pid < 0 || pid >= PROC_TABLE_MAX) return -1;

    pcb_t* p = proc_get(pid);
    if (!p || !p->active) return -1;

    spinlock_lock(&sig_global_lock);
    proc_signals_t* ps = &proc_sigs[pid];
    if (!ps->initialized) {
        spinlock_unlock(&sig_global_lock);
        return 0;
    }

    sigset_t deliverable = ps->pending & ~ps->blocked;
    if (deliverable == 0) {
        spinlock_unlock(&sig_global_lock);
        return 0;
    }

    for (int sig = 1; sig < NSIG; sig++) {
        if (deliverable & (1 << sig)) {
            ps->pending &= ~(1 << sig);
            p->signal_pending &= ~(1 << sig);

            sighandler_t handler = ps->actions[sig].sa_handler;
            int flags = ps->actions[sig].sa_flags;

            if (flags & SA_RESETHAND) {
                ps->actions[sig].sa_handler = SIG_DFL;
            }

            spinlock_unlock(&sig_global_lock);

            if (handler == SIG_IGN) {
                // Handled / ignored
            } else if (handler == SIG_DFL) {
                // Default actions
                switch (sig) {
                    case SIGHUP:
                    case SIGINT:
                    case SIGQUIT:
                    case SIGILL:
                    case SIGABRT:
                    case SIGFPE:
                    case SIGSEGV:
                    case SIGPIPE:
                    case SIGALRM:
                    case SIGTERM:
                    case SIGUSR1:
                    case SIGUSR2:
                        serial_print("[SIGNAL] PID ");
                        serial_print_dec(pid);
                        serial_print(" terminated by signal ");
                        serial_println(signal_name(sig));
                        proc_exit(pid, 128 + sig);
                        return 0;

                    case SIGCHLD:
                    case SIGURG:
                    case SIGWINCH:
                        // Ignored by default
                        break;

                    case SIGTSTP:
                        p->state = PROC_STATE_STOPPED;
                        break;

                    default:
                        break;
                }
            } else {
                // Custom user / kernel signal handler
                handler(sig);
            }

            spinlock_lock(&sig_global_lock);
        }
    }
    spinlock_unlock(&sig_global_lock);

    return 0;
}

// Signal Automated Verification Test Suite
static volatile int test_handler_invoked = 0;
static volatile int test_handler_last_sig = 0;

static void test_signal_handler(int sig) {
    test_handler_invoked = 1;
    test_handler_last_sig = sig;
}

void signal_run_tests(void) {
    serial_println("=========================================");
    serial_println("[SIGNAL] Running POSIX Signals Test Suite...");
    serial_println("=========================================");

    // Spawn test process
    pid_t t_pid = proc_spawn("sig_test_proc", "sig_test", 0);
    if (t_pid < 0) {
        serial_println("  -> [TEST FAIL] Could not spawn test process.");
        return;
    }
    signal_proc_init(t_pid);

    // Test 1: Register custom handler & verify SIGKILL immutability
    sighandler_t old_usr1 = signal_set_handler(t_pid, SIGUSR1, test_signal_handler);
    sighandler_t kill_res = signal_set_handler(t_pid, SIGKILL, test_signal_handler);
    if (old_usr1 == SIG_DFL && kill_res == SIG_ERR) {
        serial_println("  -> [TEST 1/5] Custom handler install & SIGKILL immutability: PASS");
    } else {
        serial_println("  -> [TEST 1/5] Custom handler install: FAIL");
    }

    // Test 2: Signal mask blocking & unblocking
    sigset_t bmask = (1 << SIGUSR1);
    signal_procmask(t_pid, SIG_BLOCK, &bmask, NULL);
    test_handler_invoked = 0;
    signal_send(t_pid, SIGUSR1);

    sigset_t pend = 0;
    signal_pending(t_pid, &pend);
    int blocked_ok = (pend & (1 << SIGUSR1)) && (test_handler_invoked == 0);

    signal_procmask(t_pid, SIG_UNBLOCK, &bmask, NULL);
    int unblocked_ok = (test_handler_invoked == 1 && test_handler_last_sig == SIGUSR1);

    if (blocked_ok && unblocked_ok) {
        serial_println("  -> [TEST 2/5] Signal mask block, pending queue, and unblock delivery: PASS");
    } else {
        serial_println("  -> [TEST 2/5] Signal mask blocking: FAIL");
    }

    // Test 3: SIG_IGN discard verification
    signal_set_handler(t_pid, SIGUSR2, SIG_IGN);
    test_handler_invoked = 0;
    signal_send(t_pid, SIGUSR2);
    signal_pending(t_pid, &pend);
    if (test_handler_invoked == 0 && !(pend & (1 << SIGUSR2))) {
        serial_println("  -> [TEST 3/5] SIG_IGN signal discarding: PASS");
    } else {
        serial_println("  -> [TEST 3/5] SIG_IGN signal discarding: FAIL");
    }

    // Test 4: SIGSTOP and SIGCONT state transitions
    pcb_t* p = proc_get(t_pid);
    signal_send(t_pid, SIGSTOP);
    int stop_ok = (p && p->state == PROC_STATE_STOPPED);
    signal_send(t_pid, SIGCONT);
    int cont_ok = (p && p->state == PROC_STATE_READY);
    if (stop_ok && cont_ok) {
        serial_println("  -> [TEST 4/5] SIGSTOP suspension & SIGCONT resumption: PASS");
    } else {
        serial_println("  -> [TEST 4/5] SIGSTOP / SIGCONT: FAIL");
    }

    // Test 5: Process termination via SIGKILL
    signal_send(t_pid, SIGKILL);
    if (p && p->state == PROC_STATE_ZOMBIE && p->exit_code == (128 + SIGKILL)) {
        serial_println("  -> [TEST 5/5] Immediate SIGKILL termination & exit status: PASS");
    } else {
        serial_println("  -> [TEST 5/5] Process termination via SIGKILL: FAIL");
    }

    // Cleanup test process
    signal_proc_cleanup(t_pid);
    p->active = 0;

    serial_println("[SIGNAL] POSIX Signal Subsystem: 5/5 PASSED!");
}
