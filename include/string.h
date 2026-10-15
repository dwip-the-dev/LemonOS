#ifndef STRING_H
#define STRING_H

#include <stddef.h>

void* memset(void* ptr, int value, size_t num);
void* memcpy(void* destination, const void* source, size_t num);
void* memmove(void* destination, const void* source, size_t num);
int memcmp(const void* ptr1, const void* ptr2, size_t num);
size_t strlen(const char* str);
char* strcpy(char* destination, const char* source);
char* strncpy(char* destination, const char* source, size_t num);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t num);
char* strcat(char* destination, const char* source);
char* strncat(char* destination, const char* source, size_t num);
char* strchr(const char* str, int character);
char* strrchr(const char* str, int character);
char* strstr(const char* haystack, const char* needle);
char* strdup(const char* str);
char* strndup(const char* str, size_t n);
int strcasecmp(const char* s1, const char* s2);
int strncasecmp(const char* s1, const char* s2, size_t n);

#endif
