#ifndef GIT_H
#define GIT_H

#include <stdint.h>
#include <stddef.h>

#define GIT_MAX_INDEX_ENTRIES 32
#define GIT_MAX_COMMITS       32

typedef struct {
    char path[64];
    char sha1_hex[41];
    int  staged;
} git_index_entry_t;

typedef struct {
    char sha1_hex[41];
    char parent_hex[41];
    char author[48];
    char email[48];
    char message[80];
    char date[32];
} git_commit_info_t;

void git_init_subsystem(void);
int  git_init(const char* dir);
int  git_add(const char* file_path);
int  git_commit(const char* message);
int  git_status(char* out_buf, size_t max);
int  git_log(char* out_buf, size_t max);
int  git_branch(const char* new_branch, char* out_buf, size_t max);
int  git_checkout(const char* branch_name);
int  git_config_set(const char* key, const char* value);
int  git_cli(int argc, char** argv);

// Verification tests
void git_run_tests(void);

#endif
