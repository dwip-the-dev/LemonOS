#include "string.h"

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
