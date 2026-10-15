#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

char** environ = NULL;

#define MAX_ATEXIT 32
static void (*atexit_funcs[MAX_ATEXIT])(void);
static int atexit_count = 0;

static unsigned long next_rand = 1;

int atexit(void (*function)(void)) {
    if (!function || atexit_count >= MAX_ATEXIT) return -1;
    atexit_funcs[atexit_count++] = function;
    return 0;
}

void exit(int status) {
    while (atexit_count > 0) {
        atexit_count--;
        if (atexit_funcs[atexit_count]) {
            atexit_funcs[atexit_count]();
        }
    }
    _exit(status);
}

void abort(void) {
    _exit(128 + 6); // SIGABRT
}

// Userspace Memory Allocator (Boundary-Tag Free List on sbrk)
typedef struct mem_chunk {
    size_t size;            // Size of usable payload
    int    is_free;         // 1 if free, 0 if allocated
    struct mem_chunk* next;
    struct mem_chunk* prev;
} mem_chunk_t;

static mem_chunk_t* heap_head = NULL;

void* malloc(size_t size) {
    if (size == 0) return NULL;
    // Align size to 16 bytes
    size = (size + 15) & ~15;

    mem_chunk_t* curr = heap_head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            // Split chunk if remainder is large enough
            if (curr->size >= size + sizeof(mem_chunk_t) + 32) {
                mem_chunk_t* remainder = (mem_chunk_t*)((char*)curr + sizeof(mem_chunk_t) + size);
                remainder->size = curr->size - size - sizeof(mem_chunk_t);
                remainder->is_free = 1;
                remainder->next = curr->next;
                remainder->prev = curr;
                if (curr->next) curr->next->prev = remainder;
                curr->next = remainder;
                curr->size = size;
            }
            curr->is_free = 0;
            return (void*)((char*)curr + sizeof(mem_chunk_t));
        }
        curr = curr->next;
    }

    // Need more memory from kernel via sbrk
    size_t total_alloc = size + sizeof(mem_chunk_t);
    if (total_alloc < 65536) total_alloc = 65536; // 64KB minimum chunk

    void* p = sbrk((intptr_t)total_alloc);
    if (p == (void*)-1 || p == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    mem_chunk_t* new_chunk = (mem_chunk_t*)p;
    new_chunk->size = total_alloc - sizeof(mem_chunk_t);
    new_chunk->is_free = 0;
    new_chunk->next = NULL;
    new_chunk->prev = NULL;

    if (new_chunk->size >= size + sizeof(mem_chunk_t) + 32) {
        mem_chunk_t* remainder = (mem_chunk_t*)((char*)new_chunk + sizeof(mem_chunk_t) + size);
        remainder->size = new_chunk->size - size - sizeof(mem_chunk_t);
        remainder->is_free = 1;
        remainder->next = NULL;
        remainder->prev = new_chunk;
        new_chunk->next = remainder;
        new_chunk->size = size;
    }

    if (!heap_head) {
        heap_head = new_chunk;
    } else {
        mem_chunk_t* last = heap_head;
        while (last->next) last = last->next;
        last->next = new_chunk;
        new_chunk->prev = last;
    }

    return (void*)((char*)new_chunk + sizeof(mem_chunk_t));
}

void free(void* ptr) {
    if (!ptr) return;
    mem_chunk_t* chunk = (mem_chunk_t*)((char*)ptr - sizeof(mem_chunk_t));
    chunk->is_free = 1;

    // Coalesce forward
    if (chunk->next && chunk->next->is_free) {
        chunk->size += sizeof(mem_chunk_t) + chunk->next->size;
        chunk->next = chunk->next->next;
        if (chunk->next) chunk->next->prev = chunk;
    }

    // Coalesce backward
    if (chunk->prev && chunk->prev->is_free) {
        chunk->prev->size += sizeof(mem_chunk_t) + chunk->size;
        chunk->prev->next = chunk->next;
        if (chunk->next) chunk->next->prev = chunk->prev;
    }
}

void* calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* p = malloc(total);
    if (p) {
        memset(p, 0, total);
    }
    return p;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    mem_chunk_t* chunk = (mem_chunk_t*)((char*)ptr - sizeof(mem_chunk_t));
    if (chunk->size >= size) {
        return ptr;
    }

    void* new_ptr = malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, chunk->size);
        free(ptr);
    }
    return new_ptr;
}

// Conversions
int atoi(const char* nptr) {
    return (int)strtol(nptr, NULL, 10);
}

long atol(const char* nptr) {
    return strtol(nptr, NULL, 10);
}

long long atoll(const char* nptr) {
    return (long long)strtol(nptr, NULL, 10);
}

long strtol(const char* nptr, char** endptr, int base) {
    const char* s = nptr;
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;

    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') { s++; }

    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    } else if (base == 0) {
        base = (s[0] == '0') ? 8 : 10;
    }

    long val = 0;
    while (*s) {
        int d = -1;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'z') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') d = *s - 'A' + 10;

        if (d < 0 || d >= base) break;
        val = val * base + d;
        s++;
    }

    if (endptr) *endptr = (char*)s;
    return neg ? -val : val;
}

unsigned long strtoul(const char* nptr, char** endptr, int base) {
    return (unsigned long)strtol(nptr, endptr, base);
}

char* itoa(int value, char* str, int base) {
    if (!str || base < 2 || base > 36) return NULL;
    char* p = str;
    int is_neg = 0;

    if (value < 0 && base == 10) {
        is_neg = 1;
        value = -value;
    }

    unsigned int v = (unsigned int)value;
    char buf[36];
    int idx = 0;

    if (v == 0) buf[idx++] = '0';
    else {
        while (v > 0) {
            int rem = v % base;
            buf[idx++] = (rem < 10) ? ('0' + rem) : ('a' + rem - 10);
            v /= base;
        }
    }

    if (is_neg) *p++ = '-';
    while (idx > 0) *p++ = buf[--idx];
    *p = '\0';
    return str;
}

int abs(int j) {
    return j < 0 ? -j : j;
}

long labs(long j) {
    return j < 0 ? -j : j;
}

div_t div(int numer, int denom) {
    div_t r;
    r.quot = numer / denom;
    r.rem  = numer % denom;
    return r;
}

ldiv_t ldiv(long numer, long denom) {
    ldiv_t r;
    r.quot = numer / denom;
    r.rem  = numer % denom;
    return r;
}

int rand(void) {
    next_rand = next_rand * 1103515245 + 12345;
    return (int)((next_rand >> 16) & RAND_MAX);
}

void srand(unsigned int seed) {
    next_rand = seed;
}

// Environment
char* getenv(const char* name) {
    if (!name || !environ) return NULL;
    size_t len = strlen(name);
    for (char** env = environ; *env != NULL; env++) {
        if (strncmp(*env, name, len) == 0 && (*env)[len] == '=') {
            return *env + len + 1;
        }
    }
    return NULL;
}

int setenv(const char* name, const char* value, int overwrite) {
    if (!name || name[0] == '\0' || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }
    if (getenv(name) && !overwrite) return 0;

    size_t nlen = strlen(name);
    size_t vlen = value ? strlen(value) : 0;
    char* new_entry = (char*)malloc(nlen + vlen + 2);
    if (!new_entry) return -1;

    strcpy(new_entry, name);
    strcat(new_entry, "=");
    if (value) strcat(new_entry, value);

    // Count existing environ
    int count = 0;
    if (environ) {
        while (environ[count]) count++;
    }

    // Check if replacing existing
    for (int i = 0; i < count; i++) {
        if (strncmp(environ[i], name, nlen) == 0 && environ[i][nlen] == '=') {
            environ[i] = new_entry;
            return 0;
        }
    }

    // Append to environ
    char** new_env = (char**)malloc(sizeof(char*) * (count + 2));
    if (!new_env) return -1;
    for (int i = 0; i < count; i++) new_env[i] = environ[i];
    new_env[count] = new_entry;
    new_env[count + 1] = NULL;
    environ = new_env;
    return 0;
}

int unsetenv(const char* name) {
    if (!name || name[0] == '\0' || strchr(name, '=') || !environ) {
        errno = EINVAL;
        return -1;
    }
    size_t len = strlen(name);
    for (char** ep = environ; *ep; ep++) {
        if (strncmp(*ep, name, len) == 0 && (*ep)[len] == '=') {
            char** p = ep;
            while (*p) {
                *p = *(p + 1);
                p++;
            }
            break;
        }
    }
    return 0;
}

// Sorting & Searching
void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
    if (nmemb <= 1 || size == 0 || !base || !compar) return;
    char* b = (char*)base;
    char tmp[size];

    for (size_t i = 0; i < nmemb - 1; i++) {
        size_t min_idx = i;
        for (size_t j = i + 1; j < nmemb; j++) {
            if (compar(b + j * size, b + min_idx * size) < 0) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            memcpy(tmp, b + i * size, size);
            memcpy(b + i * size, b + min_idx * size, size);
            memcpy(b + min_idx * size, tmp, size);
        }
    }
}

void* bsearch(const void* key, const void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
    size_t l = 0, r = nmemb;
    const char* b = (const char*)base;
    while (l < r) {
        size_t mid = l + (r - l) / 2;
        int cmp = compar(key, b + mid * size);
        if (cmp == 0) return (void*)(b + mid * size);
        if (cmp < 0) r = mid;
        else l = mid + 1;
    }
    return NULL;
}
