#include "string.h"
#include "stdlib.h"

void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = (unsigned char*)ptr;
    for (size_t i = 0; i < num; i++) {
        p[i] = (unsigned char)value;
    }
    return ptr;
}

void* memcpy(void* destination, const void* source, size_t num) {
    unsigned char* dst = (unsigned char*)destination;
    const unsigned char* src = (const unsigned char*)source;
    for (size_t i = 0; i < num; i++) {
        dst[i] = src[i];
    }
    return destination;
}

void* memmove(void* destination, const void* source, size_t num) {
    unsigned char* dst = (unsigned char*)destination;
    const unsigned char* src = (const unsigned char*)source;
    
    if (dst < src) {
        for (size_t i = 0; i < num; i++) {
            dst[i] = src[i];
        }
    } else {
        for (size_t i = num; i > 0; i--) {
            dst[i - 1] = src[i - 1];
        }
    }
    return destination;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
    const unsigned char* p1 = (const unsigned char*)ptr1;
    const unsigned char* p2 = (const unsigned char*)ptr2;
    
    for (size_t i = 0; i < num; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

char* strcpy(char* destination, const char* source) {
    char* dst = destination;
    while (*source) {
        *dst++ = *source++;
    }
    *dst = '\0';
    return destination;
}

char* strncpy(char* destination, const char* source, size_t num) {
    char* dst = destination;
    size_t i;
    
    for (i = 0; i < num && source[i]; i++) {
        dst[i] = source[i];
    }
    
    for (; i < num; i++) {
        dst[i] = '\0';
    }
    
    return destination;
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

int strncmp(const char* str1, const char* str2, size_t num) {
    for (size_t i = 0; i < num; i++) {
        if (str1[i] != str2[i]) {
            return str1[i] - str2[i];
        }
        if (str1[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

char* strcat(char* destination, const char* source) {
    char* dst = destination;
    while (*dst) dst++;
    while (*source) *dst++ = *source++;
    *dst = '\0';
    return destination;
}

char* strncat(char* destination, const char* source, size_t num) {
    char* dst = destination;
    while (*dst) dst++;
    size_t i = 0;
    while (i < num && *source) {
        *dst++ = *source++;
        i++;
    }
    *dst = '\0';
    return destination;
}

char* strchr(const char* str, int character) {
    char c = (char)character;
    while (*str) {
        if (*str == c) return (char*)str;
        str++;
    }
    if (c == '\0') return (char*)str;
    return NULL;
}

char* strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    if (*needle == '\0') return (char*)haystack;

    for (; *haystack != '\0'; haystack++) {
        if (*haystack == *needle) {
            const char* h = haystack;
            const char* n = needle;
            while (*h != '\0' && *n != '\0' && *h == *n) {
                h++;
                n++;
            }
            if (*n == '\0') return (char*)haystack;
        }
    }
    return NULL;
}

char* strrchr(const char* str, int character) {
    char c = (char)character;
    const char* last = NULL;
    while (*str) {
        if (*str == c) last = str;
        str++;
    }
    if (c == '\0') return (char*)str;
    return (char*)last;
}

char* strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* copy = (char*)malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, str, len + 1);
    return copy;
}

char* strndup(const char* str, size_t n) {
    if (!str) return NULL;
    size_t len = 0;
    while (len < n && str[len]) len++;
    char* copy = (char*)malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, str, len);
    copy[len] = '\0';
    return copy;
}

static inline char to_lower_char(char c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        char c1 = to_lower_char(*s1);
        char c2 = to_lower_char(*s2);
        if (c1 != c2) return (unsigned char)c1 - (unsigned char)c2;
        s1++;
        s2++;
    }
    return (unsigned char)to_lower_char(*s1) - (unsigned char)to_lower_char(*s2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (!s1[i] || !s2[i]) {
            return (unsigned char)to_lower_char(s1[i]) - (unsigned char)to_lower_char(s2[i]);
        }
        char c1 = to_lower_char(s1[i]);
        char c2 = to_lower_char(s2[i]);
        if (c1 != c2) return (unsigned char)c1 - (unsigned char)c2;
    }
    return 0;
}
