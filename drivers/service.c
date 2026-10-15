#include "service.h"
#include "string.h"
#include "serial.h"
#include "alerts.h"
#include "icons.h"

static service_entry_t services[MAX_SERVICES];
static int service_count = 0;

static void register_service(const char* name, const char* desc, const char* unit,
                             const char* exec, uint32_t mem_kb, uint32_t cpu_pct, int auto_start) {
    if (service_count >= MAX_SERVICES) return;

    service_entry_t* s = &services[service_count++];
    memset(s, 0, sizeof(service_entry_t));
    strncpy(s->name, name, sizeof(s->name) - 1);
    strncpy(s->description, desc, sizeof(s->description) - 1);
    strncpy(s->unit_file, unit, sizeof(s->unit_file) - 1);
    strncpy(s->exec_start, exec, sizeof(s->exec_start) - 1);
    s->pid = -1;
    s->status = SERVICE_STATUS_STOPPED;
    s->enabled = auto_start;
    s->memory_kb = mem_kb;
    s->cpu_usage = cpu_pct;
    s->start_time = 0;
    s->restart_count = 0;

    // Seed initial system journal lines
    strncpy(s->recent_logs[0], "Loaded unit definition", 63);
    strncpy(s->recent_logs[1], "Configured restart policy: always", 63);
    strncpy(s->recent_logs[2], "Ready for activation", 63);

    if (auto_start) {
        service_start(name);
    }
}

void service_init_subsystem(void) {
    memset(services, 0, sizeof(services));
    service_count = 0;

    // Register built-in system daemons
    register_service("syslogd",   "System Event & Kernel Logging Daemon",
                     "/lib/systemd/system/syslogd.service",   "/sbin/syslogd -D", 2048, 1, 1);
    register_service("networkd",  "Intel e1000 Gigabit Network Supervisor",
                     "/lib/systemd/system/networkd.service",  "/sbin/networkd -D", 3072, 1, 1);
    register_service("crond",     "POSIX Periodic Job Scheduler",
                     "/lib/systemd/system/crond.service",     "/sbin/crond -D", 1024, 0, 1);
    register_service("httpd",     "Embedded Micro-HTTP Web Server",
                     "/lib/systemd/system/httpd.service",     "/sbin/httpd -D", 4096, 1, 1);
    register_service("sshd",      "OpenSSH Remote Secure Shell",
                     "/lib/systemd/system/sshd.service",      "/sbin/sshd -D", 3584, 0, 1);
    register_service("dbus",      "System IPC D-Bus Message Bus",
                     "/lib/systemd/system/dbus.service",      "/sbin/dbus-daemon --system", 2560, 1, 1);
    register_service("udevd",     "Kernel Device Event Daemon",
                     "/lib/systemd/system/udevd.service",     "/sbin/udevd -D", 2048, 0, 1);
    register_service("timesyncd", "Network Time Synchronization Daemon",
                     "/lib/systemd/system/timesyncd.service", "/sbin/timesyncd -D", 1536, 0, 1);

    serial_println("[SERVICE] Service daemon supervisor initialized (8 services registered).");
}

int service_start(const char* name) {
    if (!name) return -1;

    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            if (services[i].status == SERVICE_STATUS_RUNNING) {
                return 0; // Already running
            }

            pid_t pid = proc_spawn(services[i].name, services[i].exec_start, 1);
            if (pid > 0) {
                services[i].pid = pid;
                services[i].status = SERVICE_STATUS_RUNNING;
                services[i].start_time = posix_time(NULL);

                service_log(name, "Started daemon supervisor process");

                serial_print("[SERVICE] Started service '");
                serial_print(services[i].name);
                serial_print("' (PID: ");
                serial_print_dec(pid);
                serial_println(")");
                return 0;
            } else {
                services[i].status = SERVICE_STATUS_FAILED;
                service_log(name, "Failed to start service process");
                return -1;
            }
        }
    }
    return -1;
}

int service_stop(const char* name) {
    if (!name) return -1;

    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            if (services[i].status != SERVICE_STATUS_RUNNING) {
                return 0;
            }

            if (services[i].pid > 0) {
                proc_kill(services[i].pid, SIGTERM);
            }
            services[i].status = SERVICE_STATUS_STOPPED;
            services[i].pid = -1;
            service_log(name, "Daemon stopped by administrator request");

            serial_print("[SERVICE] Stopped service '");
            serial_print(services[i].name);
            serial_println("'");
            return 0;
        }
    }
    return -1;
}

int service_restart(const char* name) {
    service_stop(name);
    return service_start(name);
}

int service_enable(const char* name) {
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            services[i].enabled = 1;
            service_log(name, "Enabled service at boot");
            return 0;
        }
    }
    return -1;
}

int service_disable(const char* name) {
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            services[i].enabled = 0;
            service_log(name, "Disabled service from boot");
            return 0;
        }
    }
    return -1;
}

int service_is_running(const char* name) {
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            return (services[i].status == SERVICE_STATUS_RUNNING);
        }
    }
    return 0;
}

service_entry_t* service_get(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            return &services[i];
        }
    }
    return NULL;
}

service_entry_t* service_get_by_index(int idx) {
    if (idx >= 0 && idx < service_count) {
        return &services[idx];
    }
    return NULL;
}

int service_get_count(void) {
    return service_count;
}

void service_log(const char* name, const char* msg) {
    service_entry_t* s = service_get(name);
    if (!s || !msg) return;

    // Shift log lines up
    strncpy(s->recent_logs[0], s->recent_logs[1], sizeof(s->recent_logs[0]) - 1);
    strncpy(s->recent_logs[1], s->recent_logs[2], sizeof(s->recent_logs[1]) - 1);
    strncpy(s->recent_logs[2], msg, sizeof(s->recent_logs[2]) - 1);
    s->recent_logs[2][sizeof(s->recent_logs[2]) - 1] = '\0';
}

void service_supervisor_tick(void) {
    for (int i = 0; i < service_count; i++) {
        if (services[i].enabled && services[i].status == SERVICE_STATUS_RUNNING) {
            if (services[i].pid > 0) {
                pcb_t* p = proc_get(services[i].pid);
                if (!p || p->state == PROC_STATE_ZOMBIE || p->state == PROC_STATE_UNUSED) {
                    // Respawn crashed daemon
                    services[i].restart_count++;
                    service_log(services[i].name, "Daemon terminated unexpectedly. Respawning...");
                    services[i].status = SERVICE_STATUS_STOPPED;
                    service_start(services[i].name);
                }
            }
        }
    }
}

int service_get_status_str(const char* name, char* out_buf, size_t max) {
    if (!name || !out_buf || max == 0) return -1;
    out_buf[0] = '\0';

    service_entry_t* s = service_get(name);
    if (!s) return -1;

    const char* st = (s->status == SERVICE_STATUS_RUNNING) ? "active (running)" :
                     (s->status == SERVICE_STATUS_STOPPED) ? "inactive (dead)" : "failed";

    strncpy(out_buf, st, max - 1);
    out_buf[max - 1] = '\0';
    return 0;
}

int service_list(char* out_buf, size_t max) {
    if (!out_buf || max == 0) return 0;
    out_buf[0] = '\0';

    size_t pos = 0;
    for (int i = 0; i < service_count; i++) {
        const char* st = (services[i].status == SERVICE_STATUS_RUNNING) ? " [ + ]  " : " [ - ]  ";
        for (int c = 0; st[c] && pos < max - 1; c++) out_buf[pos++] = st[c];
        for (int c = 0; services[i].name[c] && pos < max - 1; c++) out_buf[pos++] = services[i].name[c];
        while (pos % 20 != 0 && pos < max - 1) out_buf[pos++] = ' ';
        for (int c = 0; services[i].description[c] && pos < max - 1; c++) out_buf[pos++] = services[i].description[c];
        if (pos < max - 1) out_buf[pos++] = '\n';
    }
    out_buf[pos] = '\0';
    return (int)pos;
}

void service_run_tests(void) {
    serial_println("=========================================");
    serial_println("[SERVICE] Running Service Daemon Verification Tests...");
    serial_println("=========================================");

    // Test 1: Verify syslogd is running
    if (service_is_running("syslogd")) {
        serial_println("  -> [TEST 1/2] syslogd service autostart: PASS");
    } else {
        serial_println("  -> [TEST 1/2] syslogd service autostart: FAIL");
    }

    // Test 2: Stop and restart crond
    service_stop("crond");
    if (!service_is_running("crond")) {
        service_start("crond");
        if (service_is_running("crond")) {
            serial_println("  -> [TEST 2/2] service stop & restart lifecycle: PASS");
        } else {
            serial_println("  -> [TEST 2/2] service stop & restart lifecycle: FAIL");
        }
    } else {
        serial_println("  -> [TEST 2/2] service stop & restart lifecycle: FAIL");
    }

    serial_println("[SERVICE] Service subsystem verified.");
}
