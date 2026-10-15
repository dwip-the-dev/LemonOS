#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Atomic Integer Operations
typedef struct {
    volatile int counter;
} atomic_t;

#define ATOMIC_INIT(i)  { (i) }

static inline int atomic_read(const atomic_t* v) {
    return v->counter;
}

static inline void atomic_set(atomic_t* v, int i) {
    v->counter = i;
}

static inline void atomic_add(atomic_t* v, int i) {
    __sync_fetch_and_add(&v->counter, i);
}

static inline void atomic_sub(atomic_t* v, int i) {
    __sync_fetch_and_sub(&v->counter, i);
}

static inline void atomic_inc(atomic_t* v) {
    __sync_fetch_and_add(&v->counter, 1);
}

static inline void atomic_dec(atomic_t* v) {
    __sync_fetch_and_sub(&v->counter, 1);
}

static inline int atomic_cmpxchg(atomic_t* v, int old_val, int new_val) {
    return __sync_val_compare_and_swap(&v->counter, old_val, new_val);
}

// Kernel Spinlocks
typedef struct {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT   { 0 }

void     spinlock_init(spinlock_t* lock);
void     spinlock_lock(spinlock_t* lock);
int      spinlock_trylock(spinlock_t* lock);
void     spinlock_unlock(spinlock_t* lock);
void     spinlock_lock_irqsave(spinlock_t* lock, uint32_t* flags);
void     spinlock_unlock_irqrestore(spinlock_t* lock, uint32_t flags);

// Kernel Mutexes
typedef struct {
    volatile int locked;
    int          owner_pid;
} mutex_t;

#define MUTEX_INIT      { 0, 0 }

void     mutex_init(mutex_t* m);
void     mutex_lock(mutex_t* m);
int      mutex_trylock(mutex_t* m);
void     mutex_unlock(mutex_t* m);

// Initialization & Verification Tests
void     sync_init(void);
void     sync_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif // SYNC_H
