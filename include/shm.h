/**
 * @file shm.h
 * @brief Shared Memory IPC and Counting Semaphores for LemonOS
 * @author Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>
 *
 * Implements SysV-compatible shared memory segments with 4KB page alignment,
 * ref-counting, zero-copy inter-process communication, and counting semaphores.
 */

#ifndef SHM_H
#define SHM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// SysV IPC Constants
#define IPC_PRIVATE  0
#define IPC_CREAT    01000
#define IPC_EXCL     02000
#define IPC_RMID     0
#define IPC_SET      1
#define IPC_STAT     2

#define SHM_RDONLY   010000
#define SHM_RND      020000

#define SHM_MAX_SEGMENTS 32
#define SHM_PAGE_SIZE    4096
#define SEM_MAX_COUNT    64

// Shared Memory Info structure (sys/shm.h compatible)
struct shmid_ds {
    size_t   shm_segsz;     // Segment size in bytes
    int      shm_cpid;      // Creator PID
    int      shm_lpid;      // Last operator PID
    int      shm_nattch;    // Number of active attaches
    uint32_t shm_atime;     // Last attach time
    uint32_t shm_dtime;     // Last detach time
    uint32_t shm_ctime;     // Creation time
};

// Subsystem Initialization
void   shm_init(void);

// Shared Memory Management API
int    shm_get(int key, size_t size, int shmflg);
void*  shm_at(int shmid, const void* shmaddr, int shmflg);
int    shm_dt(const void* shmaddr);
int    shm_ctl(int shmid, int cmd, struct shmid_ds* buf);

// Counting Semaphores API
int    sem_init(int initial_value, int max_value);
int    sem_open(const char* name, int oflag, int value);
int    sem_wait(int sem_id);
int    sem_trywait(int sem_id);
int    sem_post(int sem_id);
int    sem_getvalue(int sem_id, int* sval);
int    sem_destroy(int sem_id);

// Automated Verification Test Suite
void   shm_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif // SHM_H
