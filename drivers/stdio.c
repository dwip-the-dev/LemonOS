#include "stdio.h"
#include "vga.h"
#include "string.h"

int printf(const char* format, ...) {
    // Simple implementation - just pass to vga_print
    vga_print(format);
    return 0;
}

int sprintf(char* str, const char* format, ...) {
    // Simple implementation - just copy the format string
    strcpy(str, format);
    return strlen(format);
}

int snprintf(char* str, size_t size, const char* format, ...) {
    strncpy(str, format, size);
    if (size > 0) {
        str[size - 1] = '\0';
    }
    return strlen(format);
}

int puts(const char* str) {
    vga_println(str);
    return 0;
}

// Stub file operations
FILE* fopen(const char* filename, const char* mode) { return 0; }
int fclose(FILE* stream) { return 0; }
size_t fread(void* ptr, size_t size, size_t count, FILE* stream) { return 0; }
size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream) { return 0; }
int fseek(FILE* stream, long offset, int whence) { return 0; }
long ftell(FILE* stream) { return 0; }
