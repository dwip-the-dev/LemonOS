#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t num, size_t size);
void* realloc(void* ptr, size_t size);
void abort(void);
int abs(int x);
long labs(long x);
int rand(void);
void srand(unsigned int seed);
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
char* getenv(const char* name);
int atoi(const char* str);
long atol(const char* str);
double atof(const char* str);
int system(const char* command);
void exit(int status);

#endif
