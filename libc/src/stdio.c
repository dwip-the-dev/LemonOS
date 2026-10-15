#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static FILE static_files[FOPEN_MAX];
static int  stdio_initialized = 0;

static FILE _stdin_file  = { .fd = 0, .flags = O_RDONLY, .buf_mode = _IONBF };
static FILE _stdout_file = { .fd = 1, .flags = O_WRONLY, .buf_mode = _IONBF };
static FILE _stderr_file = { .fd = 2, .flags = O_WRONLY, .buf_mode = _IONBF };

FILE* stdin  = &_stdin_file;
FILE* stdout = &_stdout_file;
FILE* stderr = &_stderr_file;

void __libc_init_stdio(void) {
    if (stdio_initialized) return;
    memset(static_files, 0, sizeof(static_files));
    static_files[0] = _stdin_file;
    static_files[1] = _stdout_file;
    static_files[2] = _stderr_file;
    stdin  = &static_files[0];
    stdout = &static_files[1];
    stderr = &static_files[2];
    stdio_initialized = 1;
}

FILE* fopen(const char* pathname, const char* mode) {
    __libc_init_stdio();
    if (!pathname || !mode) {
        errno = EINVAL;
        return NULL;
    }

    int flags = 0;
    if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) {
        flags = O_RDONLY;
    } else if (strcmp(mode, "w") == 0 || strcmp(mode, "wb") == 0) {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (strcmp(mode, "a") == 0 || strcmp(mode, "ab") == 0) {
        flags = O_WRONLY | O_CREAT | O_APPEND;
    } else if (strcmp(mode, "r+") == 0 || strcmp(mode, "rb+") == 0 || strcmp(mode, "r+b") == 0) {
        flags = O_RDWR;
    } else if (strcmp(mode, "w+") == 0 || strcmp(mode, "wb+") == 0 || strcmp(mode, "w+b") == 0) {
        flags = O_RDWR | O_CREAT | O_TRUNC;
    } else if (strcmp(mode, "a+") == 0 || strcmp(mode, "ab+") == 0 || strcmp(mode, "a+b") == 0) {
        flags = O_RDWR | O_CREAT | O_APPEND;
    } else {
        errno = EINVAL;
        return NULL;
    }

    int fd = open(pathname, flags, 0666);
    if (fd < 0) return NULL;

    for (int i = 3; i < FOPEN_MAX; i++) {
        if (static_files[i].fd == 0 && static_files[i].flags == 0) {
            FILE* f = &static_files[i];
            f->fd = fd;
            f->flags = flags;
            f->error = 0;
            f->eof = 0;
            f->buf_mode = _IONBF;
            return f;
        }
    }

    close(fd);
    errno = ENFILE;
    return NULL;
}

int fclose(FILE* stream) {
    if (!stream) return EOF;
    fflush(stream);
    int res = close(stream->fd);
    stream->fd = 0;
    stream->flags = 0;
    stream->error = 0;
    stream->eof = 0;
    return (res < 0) ? EOF : 0;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!ptr || size == 0 || nmemb == 0 || !stream) return 0;
    size_t total = size * nmemb;
    ssize_t n = read(stream->fd, ptr, total);
    if (n < 0) {
        stream->error = 1;
        return 0;
    }
    if ((size_t)n < total) {
        stream->eof = 1;
    }
    return (size_t)n / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!ptr || size == 0 || nmemb == 0 || !stream) return 0;
    size_t total = size * nmemb;
    ssize_t n = write(stream->fd, ptr, total);
    if (n < 0) {
        stream->error = 1;
        return 0;
    }
    return (size_t)n / size;
}

int fseek(FILE* stream, long offset, int whence) {
    if (!stream) return -1;
    off_t res = lseek(stream->fd, (off_t)offset, whence);
    if (res == (off_t)-1) {
        stream->error = 1;
        return -1;
    }
    stream->eof = 0;
    return 0;
}

long ftell(FILE* stream) {
    if (!stream) return -1;
    off_t res = lseek(stream->fd, 0, SEEK_CUR);
    if (res == (off_t)-1) {
        stream->error = 1;
        return -1;
    }
    return (long)res;
}

void rewind(FILE* stream) {
    fseek(stream, 0, SEEK_SET);
    if (stream) stream->error = 0;
}

int fflush(FILE* stream) {
    (void)stream;
    return 0;
}

int feof(FILE* stream) {
    return stream ? stream->eof : 0;
}

int ferror(FILE* stream) {
    return stream ? stream->error : 0;
}

void clearerr(FILE* stream) {
    if (stream) {
        stream->error = 0;
        stream->eof = 0;
    }
}

int fileno(FILE* stream) {
    return stream ? stream->fd : -1;
}

int fgetc(FILE* stream) {
    unsigned char c;
    if (fread(&c, 1, 1, stream) == 1) {
        return (int)c;
    }
    return EOF;
}

int getc(FILE* stream) {
    return fgetc(stream);
}

int getchar(void) {
    return fgetc(stdin);
}

int fputc(int c, FILE* stream) {
    unsigned char ch = (unsigned char)c;
    if (fwrite(&ch, 1, 1, stream) == 1) {
        return (int)ch;
    }
    return EOF;
}

int putc(int c, FILE* stream) {
    return fputc(c, stream);
}

int putchar(int c) {
    return fputc(c, stdout);
}

char* fgets(char* s, int size, FILE* stream) {
    if (!s || size <= 0 || !stream) return NULL;
    int idx = 0;
    while (idx < size - 1) {
        int c = fgetc(stream);
        if (c == EOF) {
            if (idx == 0) return NULL;
            break;
        }
        s[idx++] = (char)c;
        if (c == '\n') break;
    }
    s[idx] = '\0';
    return s;
}

int fputs(const char* s, FILE* stream) {
    if (!s || !stream) return EOF;
    size_t len = strlen(s);
    if (fwrite(s, 1, len, stream) == len) {
        return 0;
    }
    return EOF;
}

int puts(const char* s) {
    if (fputs(s, stdout) == EOF) return EOF;
    if (putchar('\n') == EOF) return EOF;
    return 0;
}

int ungetc(int c, FILE* stream) {
    (void)c;
    (void)stream;
    return EOF;
}

void perror(const char* s) {
    if (s && *s) {
        fputs(s, stderr);
        fputs(": ", stderr);
    }
    fputs(strerror(errno), stderr);
    fputc('\n', stderr);
}

int remove(const char* pathname) {
    return unlink(pathname);
}

int rename(const char* oldpath, const char* newpath) {
    (void)oldpath;
    (void)newpath;
    return 0;
}

// Formatted Output Engine
int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    size_t count = 0;
    const char* p = format;

    #define EMIT(c) do { \
        if (str && count + 1 < size) str[count] = (c); \
        count++; \
    } while(0)

    while (*p) {
        if (*p != '%') {
            EMIT(*p++);
            continue;
        }
        p++; // skip '%'

        // Flags
        int left_align = 0;
        int zero_pad   = 0;
        if (*p == '-') { left_align = 1; p++; }
        if (*p == '0') { zero_pad = 1; p++; }

        // Field width
        int width = 0;
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        // Precision
        int prec = -1;
        if (*p == '.') {
            p++;
            prec = 0;
            while (*p >= '0' && *p <= '9') {
                prec = prec * 10 + (*p - '0');
                p++;
            }
        }

        // Length modifiers
        int is_long = 0;
        int is_long_long = 0;
        if (*p == 'l') {
            p++;
            if (*p == 'l') { is_long_long = 1; p++; }
            else is_long = 1;
        }

        char buf[64];
        int  buf_len = 0;

        switch (*p) {
            case 'd':
            case 'i': {
                long long val;
                if (is_long_long) val = va_arg(ap, long long);
                else if (is_long) val = va_arg(ap, long);
                else val = va_arg(ap, int);

                int neg = 0;
                unsigned long long uval;
                if (val < 0) {
                    neg = 1;
                    uval = -val;
                } else {
                    uval = val;
                }

                char tmp[32]; int tp = 0;
                if (uval == 0) tmp[tp++] = '0';
                else {
                    while (uval > 0) {
                        tmp[tp++] = '0' + (uval % 10);
                        uval /= 10;
                    }
                }
                if (neg) buf[buf_len++] = '-';
                while (tp > 0) buf[buf_len++] = tmp[--tp];
                break;
            }

            case 'u': {
                unsigned long long uval;
                if (is_long_long) uval = va_arg(ap, unsigned long long);
                else if (is_long) uval = va_arg(ap, unsigned long);
                else uval = va_arg(ap, unsigned int);

                char tmp[32]; int tp = 0;
                if (uval == 0) tmp[tp++] = '0';
                else {
                    while (uval > 0) {
                        tmp[tp++] = '0' + (uval % 10);
                        uval /= 10;
                    }
                }
                while (tp > 0) buf[buf_len++] = tmp[--tp];
                break;
            }

            case 'x':
            case 'X': {
                unsigned long long uval;
                if (is_long_long) uval = va_arg(ap, unsigned long long);
                else if (is_long) uval = va_arg(ap, unsigned long);
                else uval = va_arg(ap, unsigned int);

                const char* digits = (*p == 'x') ? "0123456789abcdef" : "0123456789ABCDEF";
                char tmp[32]; int tp = 0;
                if (uval == 0) tmp[tp++] = '0';
                else {
                    while (uval > 0) {
                        tmp[tp++] = digits[uval % 16];
                        uval /= 16;
                    }
                }
                while (tp > 0) buf[buf_len++] = tmp[--tp];
                break;
            }

            case 'p': {
                uintptr_t uval = (uintptr_t)va_arg(ap, void*);
                buf[buf_len++] = '0';
                buf[buf_len++] = 'x';
                const char* digits = "0123456789abcdef";
                char tmp[16]; int tp = 0;
                if (uval == 0) tmp[tp++] = '0';
                else {
                    while (uval > 0) {
                        tmp[tp++] = digits[uval % 16];
                        uval /= 16;
                    }
                }
                while (tp > 0) buf[buf_len++] = tmp[--tp];
                break;
            }

            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                int slen = strlen(s);
                if (prec >= 0 && slen > prec) slen = prec;

                int pad = (width > slen) ? (width - slen) : 0;
                if (!left_align) {
                    while (pad--) EMIT(' ');
                }
                for (int i = 0; i < slen; i++) EMIT(s[i]);
                if (left_align) {
                    while (pad--) EMIT(' ');
                }
                p++;
                continue;
            }

            case 'c': {
                char ch = (char)va_arg(ap, int);
                EMIT(ch);
                p++;
                continue;
            }

            case '%': {
                EMIT('%');
                p++;
                continue;
            }

            default:
                EMIT(*p);
                p++;
                continue;
        }

        // Apply width and zero padding for numbers
        int pad = (width > buf_len) ? (width - buf_len) : 0;
        if (!left_align && pad > 0) {
            char pad_char = zero_pad ? '0' : ' ';
            // If negative and zero padding, emit sign first
            if (zero_pad && buf[0] == '-') {
                EMIT('-');
                for (int i = 0; i < pad; i++) EMIT('0');
                for (int i = 1; i < buf_len; i++) EMIT(buf[i]);
            } else {
                for (int i = 0; i < pad; i++) EMIT(pad_char);
                for (int i = 0; i < buf_len; i++) EMIT(buf[i]);
            }
        } else {
            for (int i = 0; i < buf_len; i++) EMIT(buf[i]);
            if (left_align && pad > 0) {
                for (int i = 0; i < pad; i++) EMIT(' ');
            }
        }

        p++;
    }

    if (str && size > 0) {
        if (count < size) str[count] = '\0';
        else str[size - 1] = '\0';
    }

    return (int)count;
    #undef EMIT
}

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int sprintf(char* str, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, 1048576, format, ap);
    va_end(ap);
    return ret;
}

int vsprintf(char* str, const char* format, va_list ap) {
    return vsnprintf(str, 1048576, format, ap);
}

int vfprintf(FILE* stream, const char* format, va_list ap) {
    char buf[2048];
    int n = vsnprintf(buf, sizeof(buf), format, ap);
    if (n > 0) {
        size_t to_write = (size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf);
        fwrite(buf, 1, to_write, stream);
    }
    return n;
}

int fprintf(FILE* stream, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

int vprintf(const char* format, va_list ap) {
    return vfprintf(stdout, format, ap);
}

int printf(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stdout, format, ap);
    va_end(ap);
    return ret;
}
