#ifndef SERVICE_H
#define SERVICE_H

#include <stdint.h>
#include <stddef.h>
#include "proc_mgr.h"

#define SERVICE_STATUS_STOPPED 0
#define SERVICE_STATUS_RUNNING 1
#define SERVICE_STATUS_FAILED  2

#define MAX_SERVICES 16

typedef struct {
    char     name[24];
    char     description[64];
    char     unit_file[48];
    char     exec_start[64];
    pid_t    pid;
    int      status;
    int      enabled;
    uint32_t memory_kb;
    uint32_t cpu_usage;
    time_t   start_time;
    int      restart_count;
    char     recent_logs[3][64];
    void     (*worker_fn)(void);
} service_entry_t;

void             service_init_subsystem(void);
int              service_start(const char* name);
int              service_stop(const char* name);
int              service_restart(const char* name);
int              service_enable(const char* name);
int              service_disable(const char* name);
int              service_is_running(const char* name);
service_entry_t* service_get(const char* name);
service_entry_t* service_get_by_index(int idx);
int              service_get_count(void);
void             service_log(const char* name, const char* msg);
void             service_supervisor_tick(void);
int              service_get_status_str(const char* name, char* out_buf, size_t max);
int              service_list(char* out_buf, size_t max);

// Verification tests
void             service_run_tests(void);

#endif
