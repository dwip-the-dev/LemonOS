#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>

void fs_init(void);
int fs_create_file(const char* filename, const char* content);
char* fs_read_file(const char* filename);
int fs_write_file(const char* filename, const char* content);
int fs_delete_file(const char* filename);
void fs_list_files(void);
int fs_file_exists(const char* filename);
uint32_t fs_get_file_size(const char* filename);

#endif
