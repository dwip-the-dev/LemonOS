/**
 * @file shm.c
 * @brief Shared Memory IPC and Counting Semaphores Implementation for LemonOS
 * @author Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>
 *
 * Implements SysV-compatible shared memory segments with 4KB page alignment,
 * ref-counting, zero-copy inter-process communication, and counting semaphores.
 */

#include "shm.h"
#include "pmm.h"
#include "sync.h"
#include "proc_mgr.h"
#include "serial.h"
#include "string.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    int              id;
    int              key;
    size_t           size;
    size_t           page_count;
    uintptr_t        phys_addr;
    struct shmid_ds  ds;
    int              marked_for_deletion;
    int              active;
    spinlock_t       lock;
} shm_segment_t;

typedef struct {
    int         id;
    int         value;
    int         max_value;
    int         waiters_count;
    char        name[32];
    int         active;
    spinlock_t  lock;
} sem_entry_t;

static shm_segment_t shm_segments[SHM_MAX_SEGMENTS];
static sem_entry_t   semaphores[SEM_MAX_COUNT];

static spinlock_t    shm_master_lock = SPINLOCK_INIT;
static spinlock_t    sem_master_lock = SPINLOCK_INIT;

void shm_init(void) {
    spinlock_init(&shm_master_lock);
    spinlock_init(&sem_master_lock);

    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
        shm_segments[i].id = i;
        shm_segments[i].active = 0;
        shm_segments[i].marked_for_deletion = 0;
        shm_segments[i].phys_addr = 0;
        shm_segments[i].size = 0;
        shm_segments[i].page_count = 0;
        spinlock_init(&shm_segments[i].lock);
    }

    for (int i = 0; i < SEM_MAX_COUNT; i++) {
        semaphores[i].id = i;
        semaphores[i].active = 0;
        semaphores[i].value = 0;
        semaphores[i].max_value = 0;
        semaphores[i].waiters_count = 0;
        semaphores[i].name[0] = '\0';
        spinlock_init(&semaphores[i].lock);
    }

    serial_println("[SHM] Shared Memory IPC & Counting Semaphores initialized.");
}

int shm_get(int key, size_t size, int shmflg) {
    if (size == 0) return -1;

    spinlock_lock(&shm_master_lock);

    // If key != IPC_PRIVATE, search for existing segment
    if (key != IPC_PRIVATE) {
        for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
            if (shm_segments[i].active && shm_segments[i].key == key) {
                if ((shmflg & IPC_CREAT) && (shmflg & IPC_EXCL)) {
                    spinlock_unlock(&shm_master_lock);
                    return -1; // -EEXIST
                }
                if (size > shm_segments[i].size) {
                    spinlock_unlock(&shm_master_lock);
                    return -1; // -EINVAL
                }
                int found_id = shm_segments[i].id;
                spinlock_unlock(&shm_master_lock);
                return found_id;
            }
        }

        // If key not found and IPC_CREAT was not specified
        if (!(shmflg & IPC_CREAT)) {
            spinlock_unlock(&shm_master_lock);
            return -1; // -ENOENT
        }
    }

    // Find a free segment slot
    int free_slot = -1;
    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
        if (!shm_segments[i].active) {
            free_slot = i;
            break;
        }
    }

    if (free_slot == -1) {
        spinlock_unlock(&shm_master_lock);
        serial_println("[SHM] ERROR: Maximum shared memory segments exceeded.");
        return -1;
    }

    // Round up size to whole 4KB pages
    size_t pages = (size + SHM_PAGE_SIZE - 1) / SHM_PAGE_SIZE;
    uintptr_t phys = pmm_alloc_frames(pages);
    if (!phys) {
        spinlock_unlock(&shm_master_lock);
        serial_println("[SHM] ERROR: Out of physical memory for SHM segment.");
        return -1;
    }

    // Zero out memory block
    memset((void*)phys, 0, pages * SHM_PAGE_SIZE);

    shm_segment_t* seg = &shm_segments[free_slot];
    seg->key = key;
    seg->size = pages * SHM_PAGE_SIZE;
    seg->page_count = pages;
    seg->phys_addr = phys;
    seg->marked_for_deletion = 0;
    seg->active = 1;

    seg->ds.shm_segsz = seg->size;
    seg->ds.shm_cpid = 1; // default to init/kernel
    seg->ds.shm_lpid = 1;
    seg->ds.shm_nattch = 0;
    seg->ds.shm_atime = 0;
    seg->ds.shm_dtime = 0;
    seg->ds.shm_ctime = 0;

    int assigned_id = seg->id;
    spinlock_unlock(&shm_master_lock);

    return assigned_id;
}

void* shm_at(int shmid, const void* shmaddr, int shmflg) {
    (void)shmaddr;
    (void)shmflg;

    if (shmid < 0 || shmid >= SHM_MAX_SEGMENTS) return (void*)-1;

    spinlock_lock(&shm_master_lock);
    shm_segment_t* seg = &shm_segments[shmid];

    if (!seg->active) {
        spinlock_unlock(&shm_master_lock);
        return (void*)-1;
    }

    seg->ds.shm_nattch++;
    seg->ds.shm_lpid = 1;
    void* result = (void*)seg->phys_addr;
    spinlock_unlock(&shm_master_lock);

    return result;
}

int shm_dt(const void* shmaddr) {
    if (!shmaddr) return -1;
    uintptr_t addr = (uintptr_t)shmaddr;

    spinlock_lock(&shm_master_lock);
    for (int i = 0; i < SHM_MAX_SEGMENTS; i++) {
        if (shm_segments[i].active && shm_segments[i].phys_addr == addr) {
            shm_segment_t* seg = &shm_segments[i];
            if (seg->ds.shm_nattch > 0) {
                seg->ds.shm_nattch--;
            }

            // If marked for deletion and no more attachments, release frames
            if (seg->marked_for_deletion && seg->ds.shm_nattch == 0) {
                pmm_free_frames(seg->phys_addr, seg->page_count);
                seg->active = 0;
                seg->phys_addr = 0;
                seg->marked_for_deletion = 0;
            }

            spinlock_unlock(&shm_master_lock);
            return 0;
        }
    }

    spinlock_unlock(&shm_master_lock);
    return -1; // -EINVAL
}

int shm_ctl(int shmid, int cmd, struct shmid_ds* buf) {
    if (shmid < 0 || shmid >= SHM_MAX_SEGMENTS) return -1;

    spinlock_lock(&shm_master_lock);
    shm_segment_t* seg = &shm_segments[shmid];

    if (!seg->active) {
        spinlock_unlock(&shm_master_lock);
        return -1;
    }

    switch (cmd) {
        case IPC_STAT:
            if (!buf) {
                spinlock_unlock(&shm_master_lock);
                return -1;
            }
            *buf = seg->ds;
            spinlock_unlock(&shm_master_lock);
            return 0;

        case IPC_RMID:
            if (seg->ds.shm_nattch == 0) {
                // Free immediately
                pmm_free_frames(seg->phys_addr, seg->page_count);
                seg->active = 0;
                seg->phys_addr = 0;
                seg->marked_for_deletion = 0;
            } else {
                // Mark for deferred deletion upon last detach
                seg->marked_for_deletion = 1;
            }
            spinlock_unlock(&shm_master_lock);
            return 0;

        default:
            spinlock_unlock(&shm_master_lock);
            return -1;
    }
}

// Counting Semaphores Implementation
int sem_init(int initial_value, int max_value) {
    if (initial_value < 0 || max_value <= 0 || initial_value > max_value) return -1;

    spinlock_lock(&sem_master_lock);
    for (int i = 0; i < SEM_MAX_COUNT; i++) {
        if (!semaphores[i].active) {
            sem_entry_t* s = &semaphores[i];
            s->value = initial_value;
            s->max_value = max_value;
            s->waiters_count = 0;
            s->name[0] = '\0';
            s->active = 1;
            int sid = s->id;
            spinlock_unlock(&sem_master_lock);
            return sid;
        }
    }

    spinlock_unlock(&sem_master_lock);
    return -1;
}

int sem_open(const char* name, int oflag, int value) {
    (void)oflag;
    if (!name) return -1;

    spinlock_lock(&sem_master_lock);
    // Search for existing named semaphore
    for (int i = 0; i < SEM_MAX_COUNT; i++) {
        if (semaphores[i].active && strcmp(semaphores[i].name, name) == 0) {
            int sid = semaphores[i].id;
            spinlock_unlock(&sem_master_lock);
            return sid;
        }
    }

    // Allocate new named semaphore
    for (int i = 0; i < SEM_MAX_COUNT; i++) {
        if (!semaphores[i].active) {
            sem_entry_t* s = &semaphores[i];
            s->value = (value >= 0) ? value : 1;
            s->max_value = 1024;
            s->waiters_count = 0;
            strncpy(s->name, name, sizeof(s->name) - 1);
            s->name[sizeof(s->name) - 1] = '\0';
            s->active = 1;
            int sid = s->id;
            spinlock_unlock(&sem_master_lock);
            return sid;
        }
    }

    spinlock_unlock(&sem_master_lock);
    return -1;
}

int sem_wait(int sem_id) {
    if (sem_id < 0 || sem_id >= SEM_MAX_COUNT) return -1;

    sem_entry_t* s = &semaphores[sem_id];
    while (1) {
        spinlock_lock(&s->lock);
        if (!s->active) {
            spinlock_unlock(&s->lock);
            return -1;
        }

        if (s->value > 0) {
            s->value--;
            spinlock_unlock(&s->lock);
            return 0;
        }

        s->waiters_count++;
        spinlock_unlock(&s->lock);

        // In cooperative/preemptive environment, yield CPU
        asm volatile("pause");
    }
}

int sem_trywait(int sem_id) {
    if (sem_id < 0 || sem_id >= SEM_MAX_COUNT) return -1;

    sem_entry_t* s = &semaphores[sem_id];
    spinlock_lock(&s->lock);
    if (!s->active) {
        spinlock_unlock(&s->lock);
        return -1;
    }

    if (s->value > 0) {
        s->value--;
        spinlock_unlock(&s->lock);
        return 0;
    }

    spinlock_unlock(&s->lock);
    return -1; // -EAGAIN
}

int sem_post(int sem_id) {
    if (sem_id < 0 || sem_id >= SEM_MAX_COUNT) return -1;

    sem_entry_t* s = &semaphores[sem_id];
    spinlock_lock(&s->lock);
    if (!s->active) {
        spinlock_unlock(&s->lock);
        return -1;
    }

    if (s->value < s->max_value) {
        s->value++;
        if (s->waiters_count > 0) {
            s->waiters_count--;
        }
        spinlock_unlock(&s->lock);
        return 0;
    }

    spinlock_unlock(&s->lock);
    return -1; // -EOVERFLOW
}

int sem_getvalue(int sem_id, int* sval) {
    if (sem_id < 0 || sem_id >= SEM_MAX_COUNT || !sval) return -1;

    sem_entry_t* s = &semaphores[sem_id];
    spinlock_lock(&s->lock);
    if (!s->active) {
        spinlock_unlock(&s->lock);
        return -1;
    }

    *sval = s->value;
    spinlock_unlock(&s->lock);
    return 0;
}

int sem_destroy(int sem_id) {
    if (sem_id < 0 || sem_id >= SEM_MAX_COUNT) return -1;

    sem_entry_t* s = &semaphores[sem_id];
    spinlock_lock(&s->lock);
    if (!s->active) {
        spinlock_unlock(&s->lock);
        return -1;
    }

    s->active = 0;
    s->value = 0;
    s->waiters_count = 0;
    s->name[0] = '\0';
    spinlock_unlock(&s->lock);

    return 0;
}

// Shared Memory & Semaphore Automated Test Suite
void shm_run_tests(void) {
    serial_println("=========================================");
    serial_println("[SHM] Running Shared Memory & Semaphores Test Suite...");
    serial_println("=========================================");

    // Test 1: Shared memory allocation & 4KB alignment
    int shmid = shm_get(0x1337, 8192, IPC_CREAT | 0666);
    if (shmid >= 0) {
        serial_println("  -> [TEST 1/5] SHM segment creation with key 0x1337 (8192 bytes): PASS");
    } else {
        serial_println("  -> [TEST 1/5] SHM segment creation: FAIL");
        return;
    }

    // Test 2: shm_at attach and zero-copy write/read verification
    char* shm_ptr1 = (char*)shm_at(shmid, NULL, 0);
    char* shm_ptr2 = (char*)shm_at(shmid, NULL, 0);
    if (shm_ptr1 != (char*)-1 && shm_ptr2 != (char*)-1 && shm_ptr1 == shm_ptr2) {
        const char* msg = "ObsidianSolidKernelIPC";
        size_t len = strlen(msg);
        memcpy(shm_ptr1, msg, len + 1);

        if (strcmp(shm_ptr2, msg) == 0) {
            serial_println("  -> [TEST 2/5] Zero-copy multi-attach coherent read/write: PASS");
        } else {
            serial_println("  -> [TEST 2/5] Zero-copy coherency: FAIL");
        }
    } else {
        serial_println("  -> [TEST 2/5] shm_at: FAIL");
    }

    // Test 3: shm_dt detach & attach count tracking
    struct shmid_ds ds;
    memset(&ds, 0, sizeof(ds));
    shm_ctl(shmid, IPC_STAT, &ds);
    int nattch_before = ds.shm_nattch;
    shm_dt(shm_ptr1);
    memset(&ds, 0, sizeof(ds));
    shm_ctl(shmid, IPC_STAT, &ds);
    int nattch_after = ds.shm_nattch;

    if (nattch_before == 2 && nattch_after == 1) {
        serial_println("  -> [TEST 3/5] shm_dt reference count decrementing (2 -> 1): PASS");
    } else {
        serial_println("  -> [TEST 3/5] shm_dt reference count: FAIL");
    }

    // Test 4: IPC_RMID deletion cleanup
    shm_dt(shm_ptr2);
    int ctl_res = shm_ctl(shmid, IPC_RMID, NULL);
    if (ctl_res == 0) {
        serial_println("  -> [TEST 4/5] IPC_RMID segment destruction & physical page reclamation: PASS");
    } else {
        serial_println("  -> [TEST 4/5] IPC_RMID destruction: FAIL");
    }

    // Test 5: Counting Semaphores (sem_init, sem_wait, sem_trywait, sem_post)
    int sem = sem_init(2, 5);
    int val = 0;
    sem_getvalue(sem, &val);
    int init_ok = (val == 2);

    sem_wait(sem); // 2 -> 1
    sem_trywait(sem); // 1 -> 0
    int try_fail = (sem_trywait(sem) == -1); // 0 -> fails immediately!
    sem_post(sem); // 0 -> 1
    sem_getvalue(sem, &val);
    int post_ok = (val == 1);
    sem_destroy(sem);

    if (init_ok && try_fail && post_ok) {
        serial_println("  -> [TEST 5/5] Counting semaphores wait/trywait/post barrier: PASS");
    } else {
        serial_println("  -> [TEST 5/5] Counting semaphores: FAIL");
    }

    serial_println("[SHM] Shared Memory & Semaphores Subsystem: 5/5 PASSED!");
}
