#ifndef INIT_H
#define INIT_H

#include <stdint.h>
#include <stddef.h>
#include "proc_mgr.h"
#include "service.h"

// System Runlevels (System V & systemd targets)
#define RUNLEVEL_POWEROFF   0
#define RUNLEVEL_SINGLE     1
#define RUNLEVEL_MULTIUSER  3
#define RUNLEVEL_GRAPHICAL  5
#define RUNLEVEL_REBOOT     6

// Target Units
#define TARGET_EMERGENCY    0
#define TARGET_RESCUE       1
#define TARGET_MULTI_USER   2
#define TARGET_GRAPHICAL    3

typedef struct {
    int      current_runlevel;
    int      previous_runlevel;
    int      target;
    time_t   boot_time;
    pid_t    init_pid;
    int      failed_units_count;
    char     hostname[32];
} init_system_state_t;

// Init System Lifecycle API
void     init_system_init(void);
void     init_system_boot(void);
int      init_set_runlevel(int runlevel);
int      init_get_runlevel(void);
void     init_reap_zombies(void);
void     init_create_systemd_fs(void);

// systemctl CLI & programmatic control
int      systemctl_exec(const char* action, const char* unit, char* out_buf, size_t max);
int      systemctl_status_dump(const char* unit, char* out_buf, size_t max);

// Verification test suite
void     init_run_tests(void);

#endif
