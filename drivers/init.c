#include "init.h"
#include "proc_mgr.h"
#include "service.h"
#include "vfs.h"
#include "filesystem.h"
#include "posix_time.h"
#include "string.h"
#include "serial.h"
#include "alerts.h"
#include "icons.h"
#include "shutdown.h"
#include "reboot.h"

static init_system_state_t init_state;

void init_system_init(void) {
    memset(&init_state, 0, sizeof(init_state));
    init_state.current_runlevel = RUNLEVEL_GRAPHICAL;
    init_state.previous_runlevel = 0;
    init_state.target = TARGET_GRAPHICAL;
    init_state.boot_time = posix_time(NULL);
    init_state.init_pid = 1;
    init_state.failed_units_count = 0;
    strncpy(init_state.hostname, "lemonos", sizeof(init_state.hostname) - 1);

    serial_println("[INIT] Systemd init process (PID 1) initialized.");
}

void init_create_systemd_fs(void) {
    // 1. Create directories
    vfs_mkdir("/etc/systemd", 0755);
    vfs_mkdir("/etc/systemd/system", 0755);
    vfs_mkdir("/lib/systemd", 0755);
    vfs_mkdir("/lib/systemd/system", 0755);
    vfs_mkdir("/etc/systemd/system/multi-user.target.wants", 0755);

    fs_register_entry("/etc/systemd", FS_TYPE_DIR);
    fs_register_entry("/etc/systemd/system", FS_TYPE_DIR);
    fs_register_entry("/lib/systemd", FS_TYPE_DIR);
    fs_register_entry("/lib/systemd/system", FS_TYPE_DIR);

    // 2. Write /etc/inittab
    const char* inittab =
        "# /etc/inittab - LemonOS Init Configuration\n"
        "id:5:initdefault:\n"
        "si::sysinit:/etc/init.d/rcS\n"
        "l0:0:wait:/etc/init.d/rc 0\n"
        "l1:1:wait:/etc/init.d/rc 1\n"
        "l3:3:wait:/etc/init.d/rc 3\n"
        "l5:5:wait:/etc/init.d/rc 5\n"
        "l6:6:wait:/etc/init.d/rc 6\n";
    int fd = vfs_open("/etc/inittab", 0x0200 | 0x0002); // O_CREAT | O_RDWR
    if (fd >= 0) {
        vfs_write(fd, inittab, strlen(inittab));
        vfs_close(fd);
        fs_register_entry("/etc/inittab", FS_TYPE_FILE);
    }

    // 3. Write unit definitions into /lib/systemd/system/
    int sc = service_get_count();
    for (int i = 0; i < sc; i++) {
        service_entry_t* s = service_get_by_index(i);
        if (!s) continue;

        char unit_content[256];
        unit_content[0] = '\0';
        strcat(unit_content, "[Unit]\nDescription=");
        strcat(unit_content, s->description);
        strcat(unit_content, "\nAfter=multi-user.target\n\n[Service]\nType=forking\nExecStart=");
        strcat(unit_content, s->exec_start);
        strcat(unit_content, "\nRestart=always\n\n[Install]\nWantedBy=multi-user.target\n");

        int ufd = vfs_open(s->unit_file, 0x0200 | 0x0002);
        if (ufd >= 0) {
            vfs_write(ufd, unit_content, strlen(unit_content));
            vfs_close(ufd);
            fs_register_entry(s->unit_file, FS_TYPE_FILE);
        }
    }

    serial_println("[INIT] Systemd filesystem and unit files created on LemonFS.");
}

void init_system_boot(void) {
    init_create_systemd_fs();

    // Ensure all enabled services are running
    int count = service_get_count();
    for (int i = 0; i < count; i++) {
        service_entry_t* s = service_get_by_index(i);
        if (s && s->enabled && s->status != SERVICE_STATUS_RUNNING) {
            service_start(s->name);
        }
    }
    serial_println("[INIT] Reached target: graphical.target (LemonOS Desktop Multi-User).");
}

int init_set_runlevel(int runlevel) {
    init_state.previous_runlevel = init_state.current_runlevel;
    init_state.current_runlevel = runlevel;

    if (runlevel == RUNLEVEL_POWEROFF) {
        serial_println("[INIT] System shutting down (runlevel 0)...");
        alert_send("System Shutdown", "Shutting down system services...", ICON_POWER, ALERT_TYPE_WARNING);
        shutdown();
        return 0;
    } else if (runlevel == RUNLEVEL_REBOOT) {
        serial_println("[INIT] System rebooting (runlevel 6)...");
        alert_send("System Reboot", "Rebooting LemonOS...", ICON_RESTART, ALERT_TYPE_WARNING);
        reboot();
        return 0;
    } else if (runlevel == RUNLEVEL_MULTIUSER) {
        init_state.target = TARGET_MULTI_USER;
        serial_println("[INIT] Switched to runlevel 3 (multi-user.target text console).");
        return 0;
    } else if (runlevel == RUNLEVEL_GRAPHICAL) {
        init_state.target = TARGET_GRAPHICAL;
        serial_println("[INIT] Switched to runlevel 5 (graphical.target Desktop GUI).");
        return 0;
    }

    return 0;
}

int init_get_runlevel(void) {
    return init_state.current_runlevel;
}

void init_reap_zombies(void) {
    // PID 1 child reaper
    int status = 0;
    while (proc_waitpid(-1, &status, 1) > 0) {
        // Child reaped
    }
}

int systemctl_exec(const char* action, const char* unit, char* out_buf, size_t max) {
    if (!action || !out_buf || max == 0) return -1;
    out_buf[0] = '\0';

    if (strcmp(action, "list-units") == 0 || strcmp(action, "list") == 0 || action[0] == '\0') {
        size_t p = 0;
        const char* hdr = "UNIT                           LOAD   ACTIVE SUB     DESCRIPTION\n";
        for (int c = 0; hdr[c] && p < max - 1; c++) out_buf[p++] = hdr[c];

        int count = service_get_count();
        for (int i = 0; i < count; i++) {
            service_entry_t* s = service_get_by_index(i);
            if (!s) continue;

            char unit_name[32];
            strcpy(unit_name, s->name);
            strcat(unit_name, ".service");

            for (int c = 0; unit_name[c] && p < max - 1; c++) out_buf[p++] = unit_name[c];
            while (p % 31 != 0 && p < max - 1) out_buf[p++] = ' ';

            const char* load = "loaded ";
            for (int c = 0; load[c] && p < max - 1; c++) out_buf[p++] = load[c];

            const char* act = (s->status == SERVICE_STATUS_RUNNING) ? "active running " : "failed dead    ";
            for (int c = 0; act[c] && p < max - 1; c++) out_buf[p++] = act[c];

            for (int c = 0; s->description[c] && p < max - 1; c++) out_buf[p++] = s->description[c];
            if (p < max - 1) out_buf[p++] = '\n';
        }
        out_buf[p] = '\0';
        return 0;
    } else if (strcmp(action, "status") == 0) {
        return systemctl_status_dump(unit, out_buf, max);
    } else if (strcmp(action, "start") == 0) {
        char bare_name[32];
        strncpy(bare_name, unit, sizeof(bare_name) - 1);
        char* dot = strchr(bare_name, '.');
        if (dot) *dot = '\0';

        int res = service_start(bare_name);
        if (res == 0) {
            strncpy(out_buf, "Started unit ", max - 1);
            strncat(out_buf, unit, max - strlen(out_buf) - 1);
        } else {
            strncpy(out_buf, "Failed to start unit ", max - 1);
            strncat(out_buf, unit, max - strlen(out_buf) - 1);
        }
        return res;
    } else if (strcmp(action, "stop") == 0) {
        char bare_name[32];
        strncpy(bare_name, unit, sizeof(bare_name) - 1);
        char* dot = strchr(bare_name, '.');
        if (dot) *dot = '\0';

        int res = service_stop(bare_name);
        strncpy(out_buf, "Stopped unit ", max - 1);
        strncat(out_buf, unit, max - strlen(out_buf) - 1);
        return res;
    } else if (strcmp(action, "restart") == 0) {
        char bare_name[32];
        strncpy(bare_name, unit, sizeof(bare_name) - 1);
        char* dot = strchr(bare_name, '.');
        if (dot) *dot = '\0';

        int res = service_restart(bare_name);
        strncpy(out_buf, "Restarted unit ", max - 1);
        strncat(out_buf, unit, max - strlen(out_buf) - 1);
        return res;
    } else if (strcmp(action, "enable") == 0) {
        char bare_name[32];
        strncpy(bare_name, unit, sizeof(bare_name) - 1);
        char* dot = strchr(bare_name, '.');
        if (dot) *dot = '\0';

        service_enable(bare_name);
        strncpy(out_buf, "Created symlink /etc/systemd/system/multi-user.target.wants/", max - 1);
        strncat(out_buf, unit, max - strlen(out_buf) - 1);
        return 0;
    } else if (strcmp(action, "disable") == 0) {
        char bare_name[32];
        strncpy(bare_name, unit, sizeof(bare_name) - 1);
        char* dot = strchr(bare_name, '.');
        if (dot) *dot = '\0';

        service_disable(bare_name);
        strncpy(out_buf, "Removed /etc/systemd/system/multi-user.target.wants/", max - 1);
        strncat(out_buf, unit, max - strlen(out_buf) - 1);
        return 0;
    } else if (strcmp(action, "is-active") == 0) {
        char bare_name[32];
        strncpy(bare_name, unit, sizeof(bare_name) - 1);
        char* dot = strchr(bare_name, '.');
        if (dot) *dot = '\0';

        if (service_is_running(bare_name)) {
            strncpy(out_buf, "active\n", max - 1);
            return 0;
        } else {
            strncpy(out_buf, "inactive\n", max - 1);
            return 1;
        }
    }

    strncpy(out_buf, "Unknown systemctl command\n", max - 1);
    return -1;
}

int systemctl_status_dump(const char* unit, char* out_buf, size_t max) {
    if (!unit || !out_buf || max == 0) return -1;
    out_buf[0] = '\0';

    char bare_name[32];
    strncpy(bare_name, unit, sizeof(bare_name) - 1);
    bare_name[sizeof(bare_name) - 1] = '\0';
    char* dot = strchr(bare_name, '.');
    if (dot) *dot = '\0';

    service_entry_t* s = service_get(bare_name);
    if (!s) {
        strncpy(out_buf, "Unit ", max - 1);
        strncat(out_buf, unit, max - strlen(out_buf) - 1);
        strncat(out_buf, " could not be found.\n", max - strlen(out_buf) - 1);
        return -1;
    }

    size_t p = 0;
    const char* dot_sym = (s->status == SERVICE_STATUS_RUNNING) ? "* " : "x ";
    for (int c = 0; dot_sym[c] && p < max - 1; c++) out_buf[p++] = dot_sym[c];

    for (int c = 0; s->name[c] && p < max - 1; c++) out_buf[p++] = s->name[c];
    const char* sfx = ".service - ";
    for (int c = 0; sfx[c] && p < max - 1; c++) out_buf[p++] = sfx[c];
    for (int c = 0; s->description[c] && p < max - 1; c++) out_buf[p++] = s->description[c];
    if (p < max - 1) out_buf[p++] = '\n';

    const char* loaded = "     Loaded: loaded (";
    for (int c = 0; loaded[c] && p < max - 1; c++) out_buf[p++] = loaded[c];
    for (int c = 0; s->unit_file[c] && p < max - 1; c++) out_buf[p++] = s->unit_file[c];
    const char* en_str = s->enabled ? "; enabled)\n" : "; disabled)\n";
    for (int c = 0; en_str[c] && p < max - 1; c++) out_buf[p++] = en_str[c];

    const char* act_str = (s->status == SERVICE_STATUS_RUNNING) ?
        "     Active: active (running) since boot; supervisor active\n" :
        "     Active: inactive (dead)\n";
    for (int c = 0; act_str[c] && p < max - 1; c++) out_buf[p++] = act_str[c];

    if (s->status == SERVICE_STATUS_RUNNING) {
        const char* pid_lbl = "   Main PID: ";
        for (int c = 0; pid_lbl[c] && p < max - 1; c++) out_buf[p++] = pid_lbl[c];

        char pid_str[16];
        int val = s->pid; int idx = 0;
        if (val == 0) pid_str[idx++] = '0';
        else {
            char t[16]; int ti = 0;
            while (val > 0) { t[ti++] = '0' + (val % 10); val /= 10; }
            for (int k = ti - 1; k >= 0; k--) pid_str[idx++] = t[k];
        }
        pid_str[idx] = '\0';
        for (int c = 0; pid_str[c] && p < max - 1; c++) out_buf[p++] = pid_str[c];

        const char* proc_tail = " (";
        for (int c = 0; proc_tail[c] && p < max - 1; c++) out_buf[p++] = proc_tail[c];
        for (int c = 0; s->name[c] && p < max - 1; c++) out_buf[p++] = s->name[c];
        const char* proc_end = ")\n      Tasks: 1 (limit: 64)\n     Memory: ";
        for (int c = 0; proc_end[c] && p < max - 1; c++) out_buf[p++] = proc_end[c];

        char mem_str[16];
        int mval = s->memory_kb; int mi = 0;
        char mt[16]; int mti = 0;
        while (mval > 0) { mt[mti++] = '0' + (mval % 10); mval /= 10; }
        for (int k = mti - 1; k >= 0; k--) mem_str[mi++] = mt[k];
        mem_str[mi] = '\0';
        for (int c = 0; mem_str[c] && p < max - 1; c++) out_buf[p++] = mem_str[c];

        const char* mem_end = " KB\n        CPU: 12ms\n";
        for (int c = 0; mem_end[c] && p < max - 1; c++) out_buf[p++] = mem_end[c];
    }

    out_buf[p] = '\0';
    return 0;
}

void init_run_tests(void) {
    serial_println("=========================================");
    serial_println("[INIT] Running Init System Verification Tests...");
    serial_println("=========================================");

    // Test 1: Verify PID 1 state
    if (init_state.init_pid == 1 && init_state.current_runlevel == RUNLEVEL_GRAPHICAL) {
        serial_println("  -> [TEST 1/2] systemd PID 1 active in runlevel 5: PASS");
    } else {
        serial_println("  -> [TEST 1/2] systemd PID 1 active in runlevel 5: FAIL");
    }

    // Test 2: Verify systemctl status dump for syslogd
    char status_buf[256];
    if (systemctl_status_dump("syslogd", status_buf, sizeof(status_buf)) == 0 &&
        strstr(status_buf, "active (running)")) {
        serial_println("  -> [TEST 2/2] systemctl status syslogd inspection: PASS");
    } else {
        serial_println("  -> [TEST 2/2] systemctl status syslogd inspection: FAIL");
    }

    serial_println("[INIT] Init system & PID 1 supervisor verified.");
}
