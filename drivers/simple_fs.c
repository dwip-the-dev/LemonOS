#include "filesystem.h"
#include "vga.h"
#include <stdint.h>

#define MAX_FILES 16
#define MAX_FILENAME 32
#define FILE_CONTENT_SIZE 1024

typedef struct {
    char name[MAX_FILENAME];
    char content[FILE_CONTENT_SIZE];
    uint32_t size;
    uint8_t exists;
} file_entry;

static file_entry files[MAX_FILES];
static uint8_t initialized = 0;

// Simple string functions
static int fs_strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

static void fs_strcpy(char* dest, const char* src) {
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

static int fs_strlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

void fs_init(void) {
    if (initialized) return;
    
    for (int i = 0; i < MAX_FILES; i++) {
        files[i].exists = 0;
        files[i].size = 0;
        files[i].name[0] = '\0';
        files[i].content[0] = '\0';
    }
    
    // Create default files
    fs_create_file("README.TXT", "Welcome to LemonOS File System!\nThis is a simple in-memory file system.\n");
    fs_create_file("HELP.TXT", "File Manager Commands:\n- list: Show files\n- view <file>: Read file\n- edit <file>: Edit file\n- create <file>: Create new file\n- delete <file>: Delete file\n- exit: Return to OS\n");
    fs_create_file("NOTES.TXT", "This is your personal notes file.\nYou can edit this text!\n");
    
    initialized = 1;
}

int fs_create_file(const char* filename, const char* content) {
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].exists) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;
    
    // Check if file already exists
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].exists && fs_strcmp(files[i].name, filename) == 0) {
            return -2;
        }
    }
    
    // Create file
    fs_strcpy(files[slot].name, filename);
    
    if (content) {
        int i = 0;
        while (content[i] && i < FILE_CONTENT_SIZE - 1) {
            files[slot].content[i] = content[i];
            i++;
        }
        files[slot].size = i;
        files[slot].content[i] = '\0';
    } else {
        files[slot].size = 0;
        files[slot].content[0] = '\0';
    }
    
    files[slot].exists = 1;
    return 0;
}

char* fs_read_file(const char* filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].exists && fs_strcmp(files[i].name, filename) == 0) {
            return files[i].content;
        }
    }
    return 0;
}

int fs_write_file(const char* filename, const char* content) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].exists && fs_strcmp(files[i].name, filename) == 0) {
            int j = 0;
            while (content[j] && j < FILE_CONTENT_SIZE - 1) {
                files[i].content[j] = content[j];
                j++;
            }
            files[i].size = j;
            files[i].content[j] = '\0';
            return 0;
        }
    }
    return -1;
}

int fs_delete_file(const char* filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].exists && fs_strcmp(files[i].name, filename) == 0) {
            files[i].exists = 0;
            files[i].name[0] = '\0';
            files[i].content[0] = '\0';
            files[i].size = 0;
            return 0;
        }
    }
    return -1;
}

void fs_list_files(void) {
    int count = 0;
    vga_println("=== Files ===");
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].exists) {
            vga_print("  ");
            vga_print(files[i].name);
            
            // Padding for alignment
            int padding = 20 - fs_strlen(files[i].name);
            for (int j = 0; j < padding; j++) vga_putchar(' ');
            
            // Print file size without vga_print_int
            char size_str[16];
            int size = files[i].size;
            int pos = 0;
            
            if (size == 0) {
                size_str[pos++] = '0';
            } else {
                // Convert number to string manually
                char temp[16];
                int temp_pos = 0;
                while (size > 0) {
                    temp[temp_pos++] = '0' + (size % 10);
                    size /= 10;
                }
                while (temp_pos > 0) {
                    size_str[pos++] = temp[--temp_pos];
                }
            }
            size_str[pos] = '\0';
            
            vga_print(size_str);
            vga_println(" bytes");
            count++;
        }
    }
    
    if (count == 0) {
        vga_println("No files found.");
    } else {
        vga_print("Total: ");
        // Print count manually
        char count_str[16];
        int count_num = count;
        int pos = 0;
        
        if (count_num == 0) {
            count_str[pos++] = '0';
        } else {
            char temp[16];
            int temp_pos = 0;
            while (count_num > 0) {
                temp[temp_pos++] = '0' + (count_num % 10);
                count_num /= 10;
            }
            while (temp_pos > 0) {
                count_str[pos++] = temp[--temp_pos];
            }
        }
        count_str[pos] = '\0';
        
        vga_print(count_str);
        vga_println(" files");
    }
}

int fs_file_exists(const char* filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].exists && fs_strcmp(files[i].name, filename) == 0) {
            return 1;
        }
    }
    return 0;
}

uint32_t fs_get_file_size(const char* filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].exists && fs_strcmp(files[i].name, filename) == 0) {
            return files[i].size;
        }
    }
    return 0;
}
