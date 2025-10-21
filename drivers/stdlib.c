#include "stdlib.h"
#include "string.h"

// Simple heap implementation
#define HEAP_SIZE 0x100000  // 1MB heap
static char heap[HEAP_SIZE];
static size_t heap_ptr = 0;

void* malloc(size_t size) {
    if (heap_ptr + size > HEAP_SIZE) {
        return 0;
    }
    void* ptr = &heap[heap_ptr];
    heap_ptr += size;
    return ptr;
}

void free(void* ptr) {
    // Simple implementation - we don't actually free memory bruh
}

void* calloc(size_t num, size_t size) {
    void* ptr = malloc(num * size);
    if (ptr) {
        memset(ptr, 0, num * size);
    }
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    // Simple implementation - just allocate new memory
    void* new_ptr = malloc(size);
    if (new_ptr && ptr) {
        memcpy(new_ptr, ptr, size);
    }
    return new_ptr;
}

void abort(void) {
    // Hang the system
    while (1) {
        asm volatile ("hlt");
    }
}

int atoi(const char* str) {
    int result = 0;
    int sign = 1;
    
    if (*str == '-') {
        sign = -1;
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

long atol(const char* str) {
    return (long)atoi(str);
}

void exit(int status) {
    // For now, just abort
    abort();
}
