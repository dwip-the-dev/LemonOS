#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdarg.h>

#define EOF          (-1)
#define BUFSIZ       1024
#define FILENAME_MAX 256
#define FOPEN_MAX    64

#define SEEK_SET     0
#define SEEK_CUR     1
#define SEEK_END     2

#define _IOFBF       0
#define _IOLBF       1
#define _IONBF       2

typedef struct _FILE {
    int   fd;
    int   flags;
    int   error;
    int   eof;
    int   buf_mode;
    char* buffer;
    int   buf_size;
    int   buf_pos;
    int   buf_len;
} FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

// File operations
FILE*  fopen(const char* pathname, const char* mode);
int    fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int    fseek(FILE* stream, long offset, int whence);
long   ftell(FILE* stream);
void   rewind(FILE* stream);
int    fflush(FILE* stream);
int    feof(FILE* stream);
int    ferror(FILE* stream);
void   clearerr(FILE* stream);
int    fileno(FILE* stream);

// Character / String I/O
int    fgetc(FILE* stream);
int    getc(FILE* stream);
int    getchar(void);
int    fputc(int c, FILE* stream);
int    putc(int c, FILE* stream);
int    putchar(int c);
char*  fgets(char* s, int size, FILE* stream);
int    fputs(const char* s, FILE* stream);
int    puts(const char* s);
int    ungetc(int c, FILE* stream);

// Formatted I/O
int    printf(const char* format, ...) __attribute__((format(printf, 1, 2)));
int    fprintf(FILE* stream, const char* format, ...) __attribute__((format(printf, 2, 3)));
int    sprintf(char* str, const char* format, ...) __attribute__((format(printf, 2, 3)));
int    snprintf(char* str, size_t size, const char* format, ...) __attribute__((format(printf, 3, 4)));

int    vprintf(const char* format, va_list ap);
int    vfprintf(FILE* stream, const char* format, va_list ap);
int    vsprintf(char* str, const char* format, va_list ap);
int    vsnprintf(char* str, size_t size, const char* format, va_list ap);

// Error output
void   perror(const char* s);

// File management
int    remove(const char* pathname);
int    rename(const char* oldpath, const char* newpath);

#endif // _STDIO_H
