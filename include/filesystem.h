#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>
#include <stddef.h>

#define FS_MAX_FILES     256
#define FS_MAX_PATH      64
#define FS_FILE_MAX_SIZE 16384

#define FS_TYPE_FILE 1
#define FS_TYPE_DIR  2

typedef struct {
    char     name[FS_MAX_PATH];
    uint32_t size;
    uint8_t  type;
    uint8_t  exists;
} fs_stat_t;

// Filesystem API
void     fs_init(void);
int      fs_create_file(const char* path, const char* content);
int      fs_create_file_bytes(const char* path, const void* data, size_t size);
char*    fs_read_file(const char* path);
int      fs_read_file_bytes(const char* path, void* buffer, size_t max_size);
int      fs_write_file(const char* path, const char* content);
int      fs_append_file(const char* path, const char* extra_content);
int      fs_delete_file(const char* path);
int      fs_file_exists(const char* path);
uint32_t fs_get_file_size(const char* path);
int      fs_mkdir(const char* dir_path);
void     fs_list_files(void);
int      fs_list_dir(const char* dir_path);
int      fs_stat(const char* path, fs_stat_t* out_stat);
int      fs_get_entry(int index, char* out_name, uint32_t* out_size, uint8_t* out_type);
int      fs_get_count(void);
int      fs_register_entry(const char* path, uint8_t type);

// Automated filesystem verification test suite
void     fs_run_tests(void);

#endif
