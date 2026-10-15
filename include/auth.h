#ifndef AUTH_H
#define AUTH_H

#include <stdint.h>
#include <stddef.h>

#define MAX_USERS 8
#define ROOT_UID  0
#define DWIP_UID  1000

typedef struct {
    uint32_t uid;
    uint32_t gid;
    char username[32];
    char realname[64];
    char home_dir[64];
    char shell[32];
    int is_sudoer;
} user_t;

void auth_init(void);
user_t* auth_get_current_user(void);
uint32_t auth_get_uid(void);
uint32_t auth_get_euid(void);
void auth_set_euid(uint32_t euid);
void auth_drop_privileges(void);
int auth_is_root(void);
int auth_is_sudoer(const char* username);
int auth_switch_user(const char* username);
const char* auth_get_prompt(void);

// User CLI command handlers
int cmd_whoami(int argc, char** argv);
int cmd_id(int argc, char** argv);
int cmd_groups(int argc, char** argv);
int cmd_passwd(int argc, char** argv);

// Utility accessors
const char* auth_get_hostname(void);
int         auth_set_hostname(const char* name);
const char* auth_get_username(void);
const char* auth_get_home(void);

#endif // AUTH_H
