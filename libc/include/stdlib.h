#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX     32767

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

// Dynamic memory allocation
void* malloc(size_t size);
void  free(void* ptr);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);

// Process termination
void exit(int status) __attribute__((noreturn));
void _exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
int  atexit(void (*function)(void));

// Conversions
int       atoi(const char* nptr);
long      atol(const char* nptr);
long long atoll(const char* nptr);
long      strtol(const char* nptr, char** endptr, int base);
unsigned long strtoul(const char* nptr, char** endptr, int base);
char*     itoa(int value, char* str, int base);

// Math / Random
int   abs(int j);
long  labs(long j);
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
int   rand(void);
void  srand(unsigned int seed);

// Environment
char* getenv(const char* name);
int   setenv(const char* name, const char* value, int overwrite);
int   unsetenv(const char* name);

// Searching & sorting
void  qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*));
void* bsearch(const void* key, const void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*));

#endif // _STDLIB_H
