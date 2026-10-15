#include "stdio.h"
#include "vga.h"
#include "serial.h"
#include "string.h"
#include "stdlib.h"
#include "vfs.h"
#include "keyboard.h"

#define STDIO_MAX_FILES 16

struct FILE {
    int fd;
    int flags;
    int eof;
    int error;
    int in_use;
};

static struct FILE s_file_pool[STDIO_MAX_FILES];

// Format an unsigned integer to string buffer in given base
static int format_uint(char* buf, size_t buf_size, uint32_t val, int base, int uppercase, int min_width, char pad) {
    char digits[] = "0123456789abcdef0123456789ABCDEF";
    const char* d = uppercase ? &digits[16] : &digits[0];
    char temp[32];
    int len = 0;
    if (val == 0) {
        temp[len++] = '0';
    } else {
        while (val > 0) {
            temp[len++] = d[val % base];
            val /= base;
        }
    }
    int total = len > min_width ? len : min_width;
    int pad_count = total - len;
    int written = 0;
    for (int i = 0; i < pad_count; i++) {
        if ((size_t)written < buf_size) buf[written] = pad;
        written++;
    }
    for (int i = len - 1; i >= 0; i--) {
        if ((size_t)written < buf_size) buf[written] = temp[i];
        written++;
    }
    return written;
}

int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    size_t written = 0;
    const char* f = format;

    while (*f) {
        if (*f != '%') {
            if (written + 1 < size && str) {
                str[written] = *f;
            }
            written++;
            f++;
            continue;
        }

        f++; // Skip '%'
        if (!*f) break;

        int left_align = 0;
        char pad = ' ';
        while (*f == '-' || *f == '0' || *f == '+') {
            if (*f == '-') left_align = 1;
            else if (*f == '0') pad = '0';
            f++;
        }

        int width = 0;
        if (*f == '*') {
            width = va_arg(ap, int);
            f++;
        } else {
            while (*f >= '0' && *f <= '9') {
                width = width * 10 + (*f - '0');
                f++;
            }
        }

        int precision = -1;
        if (*f == '.') {
            f++;
            if (*f == '*') {
                precision = va_arg(ap, int);
                f++;
            } else {
                precision = 0;
                while (*f >= '0' && *f <= '9') {
                    precision = precision * 10 + (*f - '0');
                    f++;
                }
            }
        }

        // Skip length modifiers (e.g. l, h, z)
        while (*f == 'l' || *f == 'h' || *f == 'z') {
            f++;
        }

        if (*f == '%') {
            if (written + 1 < size && str) str[written] = '%';
            written++;
        } else if (*f == 'c') {
            char c = (char)va_arg(ap, int);
            if (written + 1 < size && str) str[written] = c;
            written++;
        } else if (*f == 's') {
            const char* s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            int slen = strlen(s);
            if (precision >= 0 && precision < slen) slen = precision;
            int pad_len = (width > slen) ? (width - slen) : 0;
            if (!left_align) {
                for (int i = 0; i < pad_len; i++) {
                    if (written + 1 < size && str) str[written] = ' ';
                    written++;
                }
            }
            for (int i = 0; i < slen; i++) {
                if (written + 1 < size && str) str[written] = s[i];
                written++;
            }
            if (left_align) {
                for (int i = 0; i < pad_len; i++) {
                    if (written + 1 < size && str) str[written] = ' ';
                    written++;
                }
            }
        } else if (*f == 'o') {
            uint32_t val = va_arg(ap, uint32_t);
            if (precision >= 0) {
                pad = '0';
                if (width < precision) width = precision;
            }
            char num_buf[32];
            int n = format_uint(num_buf, sizeof(num_buf), val, 8, 0, width, pad);
            for (int i = 0; i < n; i++) {
                if (written + 1 < size && str) str[written] = num_buf[i];
                written++;
            }
        } else if (*f == 'd' || *f == 'i') {
            int val = va_arg(ap, int);
            if (val < 0) {
                if (written + 1 < size && str) str[written] = '-';
                written++;
                val = -val;
                if (width > 0) width--;
            }
            if (precision >= 0) {
                pad = '0';
                if (width < precision) width = precision;
            }
            char num_buf[32];
            int n = format_uint(num_buf, sizeof(num_buf), (uint32_t)val, 10, 0, width, pad);
            for (int i = 0; i < n; i++) {
                if (written + 1 < size && str) str[written] = num_buf[i];
                written++;
            }
        } else if (*f == 'u') {
            uint32_t val = va_arg(ap, uint32_t);
            if (precision >= 0) {
                pad = '0';
                if (width < precision) width = precision;
            }
            char num_buf[32];
            int n = format_uint(num_buf, sizeof(num_buf), val, 10, 0, width, pad);
            for (int i = 0; i < n; i++) {
                if (written + 1 < size && str) str[written] = num_buf[i];
                written++;
            }
        } else if (*f == 'x') {
            uint32_t val = va_arg(ap, uint32_t);
            if (precision >= 0) {
                pad = '0';
                if (width < precision) width = precision;
            }
            char num_buf[32];
            int n = format_uint(num_buf, sizeof(num_buf), val, 16, 0, width, pad);
            for (int i = 0; i < n; i++) {
                if (written + 1 < size && str) str[written] = num_buf[i];
                written++;
            }
        } else if (*f == 'X') {
            uint32_t val = va_arg(ap, uint32_t);
            if (precision >= 0) {
                pad = '0';
                if (width < precision) width = precision;
            }
            char num_buf[32];
            int n = format_uint(num_buf, sizeof(num_buf), val, 16, 1, width, pad);
            for (int i = 0; i < n; i++) {
                if (written + 1 < size && str) str[written] = num_buf[i];
                written++;
            }
        } else if (*f == 'p') {
            uint32_t val = (uint32_t)va_arg(ap, void*);
            if (written + 1 < size && str) str[written] = '0';
            written++;
            if (written + 1 < size && str) str[written] = 'x';
            written++;
            char num_buf[16];
            int n = format_uint(num_buf, sizeof(num_buf), val, 16, 0, 8, '0');
            for (int i = 0; i < n; i++) {
                if (written + 1 < size && str) str[written] = num_buf[i];
                written++;
            }
        } else {
            // Unknown specifier, emit as-is
            if (written + 1 < size && str) str[written] = *f;
            written++;
        }
        f++;
    }

    if (size > 0 && str) {
        if (written < size) {
            str[written] = '\0';
        } else {
            str[size - 1] = '\0';
        }
    }
    return (int)written;
}

int vsprintf(char* str, const char* format, va_list ap) {
    return vsnprintf(str, 1048576, format, ap);
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
    int ret = vsprintf(str, format, ap);
    va_end(ap);
    return ret;
}

int vfprintf(FILE* stream, const char* format, va_list ap) {
    char buf[1024];
    int ret = vsnprintf(buf, sizeof(buf), format, ap);
    if (!stream || stream == stdout || stream == stderr) {
        vga_print(buf);
        serial_print(buf);
    } else {
        fwrite(buf, 1, ret, stream);
    }
    return ret;
}

int fprintf(FILE* stream, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

int printf(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    char buf[1024];
    int ret = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    vga_print(buf);
    serial_print(buf);
    return ret;
}

int puts(const char* str) {
    vga_println(str);
    serial_println(str);
    return 0;
}

int putchar(int c) {
    char ch = (char)c;
    vga_putchar(ch);
    serial_putchar(ch);
    return c;
}

int fflush(FILE* stream) {
    (void)stream;
    return 0;
}

int remove(const char* pathname) {
    if (!pathname) return -1;
    return vfs_unlink(pathname);
}

int rename(const char* oldpath, const char* newpath) {
    if (!oldpath || !newpath) return -1;
    int fd_src = vfs_open(oldpath, O_RDONLY);
    if (fd_src < 0) return -1;
    int fd_dst = vfs_open(newpath, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd_dst < 0) {
        vfs_close(fd_src);
        return -1;
    }
    char buf[512];
    int n;
    while ((n = vfs_read(fd_src, buf, sizeof(buf))) > 0) {
        vfs_write(fd_dst, buf, n);
    }
    vfs_close(fd_src);
    vfs_close(fd_dst);
    vfs_unlink(oldpath);
    return 0;
}

// Simple sscanf / vsscanf implementation
static int is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

int vsscanf(const char* str, const char* format, va_list ap) {
    int matched = 0;
    const char* s = str;
    const char* f = format;

    while (*f && *s) {
        if (is_whitespace(*f)) {
            while (is_whitespace(*f)) f++;
            while (is_whitespace(*s)) s++;
            continue;
        }

        if (*f != '%') {
            if (*f != *s) break;
            f++;
            s++;
            continue;
        }

        f++; // Skip '%'
        if (!*f) break;

        // Skip leading whitespace in input for most specifiers (except %c)
        if (*f != 'c') {
            while (is_whitespace(*s)) s++;
        }

        if (*f == 'd' || *f == 'i') {
            int sign = 1;
            int base = 10;
            if (*s == '-') {
                sign = -1;
                s++;
            } else if (*s == '+') {
                s++;
            }
            if (*f == 'i' && *s == '0' && (*(s+1) == 'x' || *(s+1) == 'X')) {
                base = 16;
                s += 2;
            }
            int val = 0;
            int found = 0;
            while (*s) {
                int digit = -1;
                if (*s >= '0' && *s <= '9') digit = *s - '0';
                else if (base == 16 && *s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
                else if (base == 16 && *s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
                if (digit < 0 || digit >= base) break;
                val = val * base + digit;
                found = 1;
                s++;
            }
            if (!found) break;
            int* out = va_arg(ap, int*);
            if (out) *out = sign * val;
            matched++;
        } else if (*f == 'x' || *f == 'X') {
            if (*s == '0' && (*(s+1) == 'x' || *(s+1) == 'X')) s += 2;
            uint32_t val = 0;
            int found = 0;
            while (*s) {
                int digit = -1;
                if (*s >= '0' && *s <= '9') digit = *s - '0';
                else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
                else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
                if (digit < 0) break;
                val = (val << 4) | digit;
                found = 1;
                s++;
            }
            if (!found) break;
            unsigned int* out = va_arg(ap, unsigned int*);
            if (out) *out = val;
            matched++;
        } else if (*f == 'u') {
            uint32_t val = 0;
            int found = 0;
            while (*s >= '0' && *s <= '9') {
                val = val * 10 + (*s - '0');
                found = 1;
                s++;
            }
            if (!found) break;
            unsigned int* out = va_arg(ap, unsigned int*);
            if (out) *out = val;
            matched++;
        } else if (*f == 's') {
            char* out = va_arg(ap, char*);
            int idx = 0;
            while (*s && !is_whitespace(*s)) {
                if (out) out[idx++] = *s;
                s++;
            }
            if (out) out[idx] = '\0';
            if (idx == 0) break;
            matched++;
        } else if (*f == 'c') {
            char* out = va_arg(ap, char*);
            if (out) *out = *s;
            s++;
            matched++;
        } else if (*f == 'f') {
            // Approximate float parser
            float sign = 1.0f;
            if (*s == '-') { sign = -1.0f; s++; }
            else if (*s == '+') s++;
            float val = 0.0f;
            while (*s >= '0' && *s <= '9') {
                val = val * 10.0f + (*s - '0');
                s++;
            }
            if (*s == '.') {
                s++;
                float factor = 0.1f;
                while (*s >= '0' && *s <= '9') {
                    val += (*s - '0') * factor;
                    factor *= 0.1f;
                    s++;
                }
            }
            float* out = va_arg(ap, float*);
            if (out) *out = sign * val;
            matched++;
        }
        f++;
    }
    return matched;
}

int sscanf(const char* str, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsscanf(str, format, ap);
    va_end(ap);
    return ret;
}

// Real VFS-backed File Operations
FILE* fopen(const char* filename, const char* mode) {
    if (!filename || !mode) return NULL;
    int flags = 0;
    if (mode[0] == 'r') {
        if (mode[1] == '+') flags = O_RDWR;
        else flags = O_RDONLY;
    } else if (mode[0] == 'w') {
        if (mode[1] == '+') flags = O_RDWR | O_CREAT | O_TRUNC;
        else flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (mode[0] == 'a') {
        if (mode[1] == '+') flags = O_RDWR | O_CREAT | O_APPEND;
        else flags = O_WRONLY | O_CREAT | O_APPEND;
    } else {
        flags = O_RDONLY;
    }

    int fd = vfs_open(filename, flags);
    if (fd < 0) return NULL;

    for (int i = 0; i < STDIO_MAX_FILES; i++) {
        if (!s_file_pool[i].in_use) {
            s_file_pool[i].in_use = 1;
            s_file_pool[i].fd = fd;
            s_file_pool[i].flags = flags;
            s_file_pool[i].eof = 0;
            s_file_pool[i].error = 0;
            return &s_file_pool[i];
        }
    }
    vfs_close(fd);
    return NULL;
}

int fclose(FILE* stream) {
    if (!stream) return EOF;
    if (stream == stdout || stream == stderr || stream == stdin) return 0;
    int ret = vfs_close(stream->fd);
    stream->in_use = 0;
    stream->fd = -1;
    return (ret == 0) ? 0 : EOF;
}

size_t fread(void* ptr, size_t size, size_t count, FILE* stream) {
    if (!ptr || size == 0 || count == 0 || !stream) return 0;
    if (stream == stdin) {
        char* buf = (char*)ptr;
        size_t total = size * count;
        size_t bytes = 0;
        while (bytes < total) {
            poll_keyboard();
            if (has_key()) {
                char c = getchar();
                buf[bytes++] = c;
                if (c == '\n') break;
            } else if (serial_has_char()) {
                char c = serial_getchar();
                if (c == '\r') c = '\n';
                buf[bytes++] = c;
                if (c == '\n') break;
            } else {
                asm volatile("pause");
            }
        }
        return bytes / size;
    }
    int bytes = vfs_read(stream->fd, ptr, size * count);
    if (bytes < 0) {
        stream->error = 1;
        return 0;
    }
    if (bytes == 0) {
        stream->eof = 1;
        return 0;
    }
    return (size_t)bytes / size;
}

size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream) {
    if (!ptr || size == 0 || count == 0 || !stream) return 0;
    if (stream == stdout || stream == stderr) {
        const char* p = (const char*)ptr;
        size_t bytes = size * count;
        for (size_t i = 0; i < bytes; i++) {
            vga_putchar(p[i]);
            serial_putchar(p[i]);
        }
        return count;
    }
    int bytes = vfs_write(stream->fd, ptr, size * count);
    if (bytes < 0) {
        stream->error = 1;
        return 0;
    }
    return (size_t)bytes / size;
}

int fseek(FILE* stream, long offset, int whence) {
    if (!stream) return -1;
    if (stream == stdout || stream == stderr || stream == stdin) return -1;
    int ret = vfs_lseek(stream->fd, (int32_t)offset, whence);
    if (ret < 0) {
        stream->error = 1;
        return -1;
    }
    stream->eof = 0;
    return 0;
}

long ftell(FILE* stream) {
    if (!stream) return -1;
    if (stream == stdout || stream == stderr || stream == stdin) return 0;
    return (long)vfs_tell(stream->fd);
}

int fgetc(FILE* stream) {
    unsigned char ch;
    if (fread(&ch, 1, 1, stream) == 1) return (int)ch;
    return EOF;
}

int fputc(int c, FILE* stream) {
    unsigned char ch = (unsigned char)c;
    if (fwrite(&ch, 1, 1, stream) == 1) return c;
    return EOF;
}

char* fgets(char* str, int num, FILE* stream) {
    if (!str || num <= 1 || !stream) return NULL;
    int idx = 0;
    while (idx < num - 1) {
        int c = fgetc(stream);
        if (c == EOF) {
            if (idx == 0) return NULL;
            break;
        }
        str[idx++] = (char)c;
        if (c == '\n') break;
    }
    str[idx] = '\0';
    return str;
}

int fputs(const char* str, FILE* stream) {
    if (!str || !stream) return EOF;
    size_t len = strlen(str);
    if (fwrite(str, 1, len, stream) == len) return 0;
    return EOF;
}

int feof(FILE* stream) {
    return stream ? stream->eof : 1;
}

int ferror(FILE* stream) {
    return stream ? stream->error : 1;
}

void clearerr(FILE* stream) {
    if (stream) { stream->eof = 0; stream->error = 0; }
}

void rewind(FILE* stream) {
    if (stream) fseek(stream, 0, SEEK_SET);
}
