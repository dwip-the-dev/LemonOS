#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t num, size_t size);
void* realloc(void* ptr, size_t size);
void abort(void);
int atoi(const char* str);
long atol(const char* str);
void exit(int status);

#endif
