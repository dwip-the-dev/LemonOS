#include "auth.h"
#include "filesystem.h"
#include "vfs.h"
#include "string.h"
#include "serial.h"
#include "vga.h"

static user_t user_table[MAX_USERS];
static int user_count = 0;
static user_t* active_user = NULL;
static uint32_t effective_uid = DWIP_UID;

static void register_user(uint32_t uid, uint32_t gid, const char* name,
                          const char* realname, const char* home,
                          const char* shell, int is_sudoer) {
    if (user_count >= MAX_USERS) return;
    user_t* u = &user_table[user_count++];
    u->uid = uid;
    u->gid = gid;
    strncpy(u->username, name, sizeof(u->username) - 1);
    strncpy(u->realname, realname, sizeof(u->realname) - 1);
    strncpy(u->home_dir, home, sizeof(u->home_dir) - 1);
    strncpy(u->shell, shell, sizeof(u->shell) - 1);
    u->is_sudoer = is_sudoer;
}

void auth_init(void) {
    user_count = 0;

    // Register primary users
    register_user(ROOT_UID, 0, "root", "Superuser", "/root", "/bin/bash", 1);
    register_user(DWIP_UID, 1000, "dwip", "Dwip Biswas (dwip-the-dev)", "/home/dwip", "/bin/bash", 1);
    register_user(1001, 1001, "guest", "Guest User", "/home/guest", "/bin/sh", 0);

    // Default session: Dwip Biswas (normal user)
    active_user = &user_table[1]; // dwip
    effective_uid = DWIP_UID;

    // Create user home directories
    fs_mkdir("/home");
    fs_mkdir("/home/dwip");
    fs_mkdir("/home/dwip/Desktop");
    fs_mkdir("/home/dwip/Downloads");
    fs_mkdir("/home/dwip/Pictures");
    fs_mkdir("/home/dwip/Documents");
    fs_mkdir("/root");

    // Populate /etc/passwd
    fs_mkdir("/etc");
    fs_create_file("/etc/passwd",
        "root:x:0:0:root:/root:/bin/bash\n"
        "daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin\n"
        "dwip:x:1000:1000:Dwip Biswas (dwip-the-dev),,,:/home/dwip:/bin/bash\n"
        "guest:x:1001:1001:Guest User,,,:/home/guest:/bin/sh\n");

    // Populate /etc/shadow
    fs_create_file("/etc/shadow",
        "root:*::0:99999:7:::\n"
        "dwip:*::0:99999:7:::\n"
        "guest:*::0:99999:7:::\n");

    // Populate /etc/group
    fs_create_file("/etc/group",
        "root:x:0:\n"
        "daemon:x:1:\n"
        "adm:x:4:dwip\n"
        "sudo:x:27:dwip\n"
        "dwip:x:1000:\n"
        "guest:x:1001:\n");

    // Populate /etc/sudoers
    fs_create_file("/etc/sudoers",
        "# /etc/sudoers - LemonOS Userspace Rules\n"
        "# Created for Dwip (dwip-the-dev)\n"
        "root    ALL=(ALL:ALL) ALL\n"
        "%sudo   ALL=(ALL:ALL) ALL\n"
        "dwip    ALL=(ALL:ALL) NOPASSWD: ALL\n");

    serial_println("[LemonOS] Auth subsystem initialized: active user = dwip (UID 1000)");
}

user_t* auth_get_current_user(void) {
    return active_user;
}

uint32_t auth_get_uid(void) {
    return active_user ? active_user->uid : DWIP_UID;
}

uint32_t auth_get_euid(void) {
    return effective_uid;
}

void auth_set_euid(uint32_t euid) {
    effective_uid = euid;
}

void auth_drop_privileges(void) {
    if (active_user) {
        effective_uid = active_user->uid;
    } else {
        effective_uid = DWIP_UID;
    }
}

int auth_is_root(void) {
    return (effective_uid == ROOT_UID);
}

int auth_is_sudoer(const char* username) {
    if (!username) return 0;
    for (int i = 0; i < user_count; i++) {
        if (strcmp(user_table[i].username, username) == 0) {
            return user_table[i].is_sudoer;
        }
    }
    return 0;
}

int auth_switch_user(const char* username) {
    if (!username || !username[0]) {
        // Default to root
        active_user = &user_table[0];
        effective_uid = ROOT_UID;
        return 0;
    }

    for (int i = 0; i < user_count; i++) {
        if (strcmp(user_table[i].username, username) == 0) {
            active_user = &user_table[i];
            effective_uid = active_user->uid;
            return 0;
        }
    }
    return -1;
}

const char* auth_get_prompt(void) {
    if (effective_uid == ROOT_UID) {
        return "root@lemonos:~# ";
    }
    return "dwip@lemonos:~$ ";
}

int cmd_whoami(int argc, char** argv) {
    (void)argc; (void)argv;
    if (active_user) {
        vga_println(active_user->username);
    } else {
        vga_println("unknown");
    }
    return 0;
}

static void auth_print_dec(uint32_t val) {
    if (val == 0) { vga_putchar('0'); return; }
    char buf[16]; int idx = 0;
    while (val > 0) { buf[idx++] = '0' + (val % 10); val /= 10; }
    for (int i = idx - 1; i >= 0; i--) vga_putchar(buf[i]);
}

int cmd_id(int argc, char** argv) {
    (void)argc; (void)argv;
    if (!active_user) { vga_println("uid=0(root) gid=0(root)"); return 0; }
    vga_print("uid=");
    auth_print_dec(active_user->uid);
    vga_print("("); vga_print(active_user->username); vga_print(") ");
    vga_print("gid=");
    auth_print_dec(active_user->gid);
    vga_print("("); vga_print(active_user->username); vga_print(") ");
    vga_print("euid=");
    auth_print_dec(effective_uid);
    if (effective_uid == ROOT_UID) {
        vga_print("(root)");
    } else {
        vga_print("("); vga_print(active_user->username); vga_print(")");
    }
    if (active_user->is_sudoer) {
        vga_print(" groups=");
        auth_print_dec(active_user->gid);
        vga_print("("); vga_print(active_user->username); vga_print("),4(adm),27(sudo)");
    }
    vga_println("");
    return 0;
}

int cmd_groups(int argc, char** argv) {
    (void)argc; (void)argv;
    if (!active_user) { vga_println("root"); return 0; }
    vga_print(active_user->username);
    vga_print(" : ");
    vga_print(active_user->username);
    if (active_user->is_sudoer) {
        vga_print(" adm sudo");
    }
    vga_println("");
    return 0;
}

int cmd_passwd(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_print("Changing password for ");
    vga_print(active_user ? active_user->username : "root");
    vga_println(".");
    vga_println("passwd: password updated successfully");
    return 0;
}

static char system_hostname[64] = "lemonos";

const char* auth_get_hostname(void) {
    return system_hostname;
}

int auth_set_hostname(const char* name) {
    if (!name || name[0] == '\0') return -1;
    strncpy(system_hostname, name, sizeof(system_hostname) - 1);
    system_hostname[sizeof(system_hostname) - 1] = '\0';
    fs_create_file("/etc/hostname", system_hostname);
    return 0;
}

const char* auth_get_username(void) {
    if (active_user) return active_user->username;
    return "root";
}

const char* auth_get_home(void) {
    if (active_user) return active_user->home_dir;
    return "/root";
}
