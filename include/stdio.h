#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>

#define EOF (-1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct FILE FILE;

#define stdin  ((FILE*)0)
#define stdout ((FILE*)1)
#define stderr ((FILE*)2)

int printf(const char* format, ...);
int fprintf(FILE* stream, const char* format, ...);
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t size, const char* format, ...);
int vsprintf(char* str, const char* format, va_list ap);
int vsnprintf(char* str, size_t size, const char* format, va_list ap);
int vfprintf(FILE* stream, const char* format, va_list ap);

int puts(const char* str);
int putchar(int c);
int fflush(FILE* stream);
int remove(const char* pathname);
int rename(const char* oldpath, const char* newpath);

int sscanf(const char* str, const char* format, ...);
int vsscanf(const char* str, const char* format, va_list ap);

// File operations
FILE* fopen(const char* filename, const char* mode);
int fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t count, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
int fgetc(FILE* stream);
int fputc(int c, FILE* stream);
char* fgets(char* str, int num, FILE* stream);
int fputs(const char* str, FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
void clearerr(FILE* stream);
void rewind(FILE* stream);

#endif
