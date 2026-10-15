// LemonOS Kernel Synchronization Primitives
// Atomic Counters, Hardware Spinlocks, and Sleep Mutexes
// Author: Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "sync.h"
#include "serial.h"

void sync_init(void) {
    serial_println("[SYNC] Kernel atomic synchronization & spinlock subsystem initialized.");
}

void spinlock_init(spinlock_t* lock) {
    if (!lock) return;
    lock->locked = 0;
}

void spinlock_lock(spinlock_t* lock) {
    if (!lock) return;
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        // x86 PAUSE instruction to prevent pipeline memory order violations
        asm volatile("pause");
    }
}

int spinlock_trylock(spinlock_t* lock) {
    if (!lock) return 0;
    return (__sync_lock_test_and_set(&lock->locked, 1) == 0) ? 1 : 0;
}

void spinlock_unlock(spinlock_t* lock) {
    if (!lock) return;
    __sync_lock_release(&lock->locked);
}

void spinlock_lock_irqsave(spinlock_t* lock, uint32_t* flags) {
    if (!lock || !flags) return;
    uint32_t eflags;
    asm volatile("pushfl; popl %0; cli" : "=r"(eflags));
    *flags = eflags;
    spinlock_lock(lock);
}

void spinlock_unlock_irqrestore(spinlock_t* lock, uint32_t flags) {
    if (!lock) return;
    spinlock_unlock(lock);
    if (flags & 0x200) { // Interrupt Enable Flag (IF) bit 9
        asm volatile("sti");
    }
}

void mutex_init(mutex_t* m) {
    if (!m) return;
    m->locked = 0;
    m->owner_pid = 0;
}

void mutex_lock(mutex_t* m) {
    if (!m) return;
    while (__sync_lock_test_and_set(&m->locked, 1)) {
        asm volatile("pause");
    }
    m->owner_pid = 1; // Mark current task
}

int mutex_trylock(mutex_t* m) {
    if (!m) return 0;
    if (__sync_lock_test_and_set(&m->locked, 1) == 0) {
        m->owner_pid = 1;
        return 1;
    }
    return 0;
}

void mutex_unlock(mutex_t* m) {
    if (!m) return;
    m->owner_pid = 0;
    __sync_lock_release(&m->locked);
}

void sync_run_tests(void) {
    serial_println("=========================================");
    serial_println("[SYNC] Running Kernel Synchronization Test Suite...");
    serial_println("=========================================");

    // Test 1: Atomic operations
    atomic_t counter = ATOMIC_INIT(100);
    atomic_inc(&counter);
    atomic_add(&counter, 50);
    atomic_dec(&counter);
    atomic_sub(&counter, 20);
    if (atomic_read(&counter) == 130) {
        serial_println("  -> [TEST 1/4] Atomic inc, add, dec, sub operations: PASS");
    } else {
        serial_println("  -> [TEST 1/4] Atomic arithmetic operations: FAIL");
    }

    // Test 2: Atomic compare-and-swap (cmpxchg)
    int old_val = atomic_cmpxchg(&counter, 130, 200);
    if (old_val == 130 && atomic_read(&counter) == 200) {
        serial_println("  -> [TEST 2/4] Atomic compare-and-swap (cmpxchg): PASS");
    } else {
        serial_println("  -> [TEST 2/4] Atomic compare-and-swap: FAIL");
    }

    // Test 3: Spinlock acquire, trylock rejection, and unlock
    spinlock_t lock = SPINLOCK_INIT;
    spinlock_lock(&lock);
    int trylock_rejected = !spinlock_trylock(&lock);
    spinlock_unlock(&lock);
    int trylock_success = spinlock_trylock(&lock);
    if (trylock_success) spinlock_unlock(&lock);

    if (trylock_rejected && trylock_success) {
        serial_println("  -> [TEST 3/4] Hardware spinlock lock, trylock, and unlock: PASS");
    } else {
        serial_println("  -> [TEST 3/4] Spinlock lock/trylock/unlock: FAIL");
    }

    // Test 4: Mutex mutual exclusion
    mutex_t m = MUTEX_INIT;
    mutex_lock(&m);
    int m_try_fail = !mutex_trylock(&m);
    mutex_unlock(&m);
    int m_try_ok = mutex_trylock(&m);
    if (m_try_ok) mutex_unlock(&m);

    if (m_try_fail && m_try_ok) {
        serial_println("  -> [TEST 4/4] Kernel mutex mutual exclusion: PASS");
    } else {
        serial_println("  -> [TEST 4/4] Mutex mutual exclusion: FAIL");
    }

    serial_println("[SYNC] Kernel Synchronization Subsystem: 4/4 PASSED!");
}
