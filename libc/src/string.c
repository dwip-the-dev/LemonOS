#include <string.h>
#include <stdlib.h>
#include <errno.h>

void* memcpy(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    unsigned char val = (unsigned char)c;
    while (n--) {
        *p++ = val;
    }
    return s;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

void* memchr(const void* s, int c, size_t n) {
    const unsigned char* p = (const unsigned char*)s;
    unsigned char val = (unsigned char)c;
    while (n--) {
        if (*p == val) return (void*)p;
        p++;
    }
    return NULL;
}

void bzero(void* s, size_t n) {
    memset(s, 0, n);
}

size_t strlen(const char* s) {
    size_t len = 0;
    while (s && s[len]) len++;
    return len;
}

size_t strnlen(const char* s, size_t maxlen) {
    size_t len = 0;
    while (s && len < maxlen && s[len]) len++;
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* orig = dest;
    while ((*dest++ = *src++));
    return orig;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* orig = dest;
    while (*dest) dest++;
    while ((*dest++ = *src++));
    return orig;
}

char* strncat(char* dest, const char* src, size_t n) {
    char* orig = dest;
    while (*dest) dest++;
    while (n-- && *src) *dest++ = *src++;
    *dest = '\0';
    return orig;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n-- && *s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    if ((int)n < 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static int tolower_internal(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && (tolower_internal((unsigned char)*s1) == tolower_internal((unsigned char)*s2))) {
        s1++; s2++;
    }
    return tolower_internal((unsigned char)*s1) - tolower_internal((unsigned char)*s2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
    while (n-- && *s1 && (tolower_internal((unsigned char)*s1) == tolower_internal((unsigned char)*s2))) {
        s1++; s2++;
    }
    if ((int)n < 0) return 0;
    return tolower_internal((unsigned char)*s1) - tolower_internal((unsigned char)*s2);
}

char* strchr(const char* s, int c) {
    char ch = (char)c;
    while (*s) {
        if (*s == ch) return (char*)s;
        s++;
    }
    if (ch == '\0') return (char*)s;
    return NULL;
}

char* strrchr(const char* s, int c) {
    char ch = (char)c;
    const char* last = NULL;
    while (*s) {
        if (*s == ch) last = s;
        s++;
    }
    if (ch == '\0') return (char*)s;
    return (char*)last;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char* h = haystack;
            const char* n = needle;
            while (*h && *n && *h == *n) {
                h++; n++;
            }
            if (!*n) return (char*)haystack;
        }
    }
    return NULL;
}

char* strtok_r(char* str, const char* delim, char** saveptr) {
    char* s = str ? str : *saveptr;
    if (!s) return NULL;

    while (*s && strchr(delim, *s)) s++;
    if (!*s) {
        *saveptr = NULL;
        return NULL;
    }

    char* token = s;
    while (*s && !strchr(delim, *s)) s++;
    if (*s) {
        *s = '\0';
        *saveptr = s + 1;
    } else {
        *saveptr = NULL;
    }
    return token;
}

char* strtok(char* str, const char* delim) {
    static char* static_saveptr = NULL;
    return strtok_r(str, delim, &static_saveptr);
}

size_t strspn(const char* s, const char* accept) {
    size_t count = 0;
    while (*s && strchr(accept, *s)) {
        count++;
        s++;
    }
    return count;
}

size_t strcspn(const char* s, const char* reject) {
    size_t count = 0;
    while (*s && !strchr(reject, *s)) {
        count++;
        s++;
    }
    return count;
}

char* strpbrk(const char* s, const char* accept) {
    while (*s) {
        if (strchr(accept, *s)) return (char*)s;
        s++;
    }
    return NULL;
}

char* strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* copy = (char*)malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

char* strndup(const char* s, size_t n) {
    if (!s) return NULL;
    size_t len = strnlen(s, n);
    char* copy = (char*)malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len);
        copy[len] = '\0';
    }
    return copy;
}

char* strerror(int errnum) {
    switch (errnum) {
        case 0:           return "Success";
        case EPERM:       return "Operation not permitted";
        case ENOENT:      return "No such file or directory";
        case ESRCH:       return "No such process";
        case EINTR:       return "Interrupted system call";
        case EIO:         return "Input/output error";
        case ENXIO:       return "No such device or address";
        case E2BIG:       return "Argument list too long";
        case ENOEXEC:     return "Exec format error";
        case EBADF:       return "Bad file descriptor";
        case ECHILD:      return "No child processes";
        case EAGAIN:      return "Resource temporarily unavailable";
        case ENOMEM:      return "Cannot allocate memory";
        case EACCES:      return "Permission denied";
        case EFAULT:      return "Bad address";
        case EBUSY:       return "Device or resource busy";
        case EEXIST:      return "File exists";
        case EXDEV:       return "Invalid cross-device link";
        case ENODEV:      return "No such device";
        case ENOTDIR:     return "Not a directory";
        case EISDIR:      return "Is a directory";
        case EINVAL:      return "Invalid argument";
        case ENFILE:      return "Too many open files in system";
        case EMFILE:      return "Too many open files";
        case ENOTTY:      return "Inappropriate ioctl for device";
        case EFBIG:       return "File too large";
        case ENOSPC:      return "No space left on device";
        case ESPIPE:      return "Illegal seek";
        case EROFS:       return "Read-only file system";
        case EMLINK:      return "Too many links";
        case EPIPE:       return "Broken pipe";
        case EDOM:        return "Numerical argument out of domain";
        case ERANGE:      return "Numerical result out of range";
        case ENOSYS:      return "Function not implemented";
        default:          return "Unknown error";
    }
}
