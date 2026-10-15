#ifndef LEMON_USER_H
#define LEMON_USER_H

#include <stdint.h>
#include <stddef.h>

#define SYS_EXIT         1
#define SYS_FORK         2
#define SYS_READ         3
#define SYS_WRITE        4
#define SYS_OPEN         5
#define SYS_CLOSE        6
#define SYS_WAITPID      7
#define SYS_CREAT        8
#define SYS_UNLINK       10
#define SYS_EXEC         11
#define SYS_TIME         13
#define SYS_STAT         18
#define SYS_LSEEK        19
#define SYS_GETPID       20
#define SYS_MKDIR        39
#define SYS_BRK          45
#define SYS_PRINT        200
#define SYS_GETC         201
#define SYS_GFX_INFO     202
#define SYS_GFX_DRAW     203
#define SYS_GET_TICKS_MS 204
#define SYS_SLEEP_MS     205
#define SYS_POLL_KEY     206
#define SYS_AUDIO_OPEN   207
#define SYS_AUDIO_WRITE  208
#define SYS_AUDIO_CLOSE  209
#define SYS_YIELD        158

// Framebuffer Display Specification
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
    uint32_t fb_address;
} lemon_display_t;

// Inline Assembly Syscall Wrappers (int 0x80)
static inline int syscall0(int num) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num) : "memory");
    return ret;
}

static inline int syscall1(int num, uint32_t a1) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1) : "memory");
    return ret;
}

static inline int syscall2(int num, uint32_t a1, uint32_t a2) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2) : "memory");
    return ret;
}

static inline int syscall3(int num, uint32_t a1, uint32_t a2, uint32_t a3) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2), "d"(a3) : "memory");
    return ret;
}

static inline int syscall4(int num, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2), "d"(a3), "S"(a4) : "memory");
    return ret;
}

static inline int syscall5(int num, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2), "d"(a3), "S"(a4), "D"(a5) : "memory");
    return ret;
}

// User POSIX API Wrappers
static inline void sys_exit(int code) {
    syscall1(SYS_EXIT, (uint32_t)code);
    while (1) {}
}

static inline int sys_write(int fd, const void* buf, size_t count) {
    return syscall3(SYS_WRITE, (uint32_t)fd, (uint32_t)buf, (uint32_t)count);
}

static inline int sys_read(int fd, void* buf, size_t count) {
    return syscall3(SYS_READ, (uint32_t)fd, (uint32_t)buf, (uint32_t)count);
}

static inline int sys_open(const char* path, int flags) {
    return syscall2(SYS_OPEN, (uint32_t)path, (uint32_t)flags);
}

static inline int sys_close(int fd) {
    return syscall1(SYS_CLOSE, (uint32_t)fd);
}

static inline int sys_getpid(void) {
    return syscall0(SYS_GETPID);
}

static inline void* sys_sbrk(intptr_t increment) {
    return (void*)syscall1(SYS_BRK, (uint32_t)increment);
}

static inline int sys_print(const char* str) {
    return syscall1(SYS_PRINT, (uint32_t)str);
}

static inline char sys_getc(void) {
    return (char)syscall0(SYS_GETC);
}

static inline int sys_time(void) {
    return syscall0(SYS_TIME);
}

static inline int sys_yield(void) {
    return syscall0(SYS_YIELD);
}

// Framebuffer & Graphics API
static inline lemon_display_t lemon_display_get(void) {
    lemon_display_t disp = {0, 0, 0, 0, 0};
    syscall1(SYS_GFX_INFO, (uint32_t)&disp);
    return disp;
}

static inline void lemon_display_blit(const void* src, uint32_t w, uint32_t h) {
    syscall3(SYS_GFX_DRAW, (uint32_t)src, w, h);
}

// Monotonic Timing API
static inline uint32_t sys_get_ticks_ms(void) {
    return (uint32_t)syscall0(SYS_GET_TICKS_MS);
}

static inline void sys_sleep_ms(uint32_t ms) {
    syscall1(SYS_SLEEP_MS, ms);
}

// Keyboard Event API
static inline int sys_poll_key_event(int* pressed, unsigned char* doom_key) {
    return syscall2(SYS_POLL_KEY, (uint32_t)pressed, (uint32_t)doom_key);
}

// Audio Stream API
static inline int sys_audio_open(uint32_t sample_rate, uint8_t format, uint8_t channels) {
    return syscall3(SYS_AUDIO_OPEN, sample_rate, (uint32_t)format, (uint32_t)channels);
}

static inline int sys_audio_write(const void* buf, size_t count) {
    return syscall2(SYS_AUDIO_WRITE, (uint32_t)buf, (uint32_t)count);
}

static inline void sys_audio_close(void) {
    syscall0(SYS_AUDIO_CLOSE);
}

// Userspace Boundary-Tag Dynamic Memory Allocator
typedef struct user_mem_block {
    size_t size;
    int is_free;
    struct user_mem_block* next;
    struct user_mem_block* prev;
} user_mem_block_t;

static user_mem_block_t* user_heap_head = NULL;

static inline void* user_malloc(size_t size) {
    if (size == 0) return NULL;
    size = (size + 15) & ~15;

    user_mem_block_t* curr = user_heap_head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            curr->is_free = 0;
            return (void*)((uint8_t*)curr + sizeof(user_mem_block_t));
        }
        curr = curr->next;
    }

    size_t total_needed = size + sizeof(user_mem_block_t);
    if (total_needed < 65536) total_needed = 65536;
    void* p = sys_sbrk((intptr_t)total_needed);
    if ((intptr_t)p <= 0) return NULL;

    user_mem_block_t* new_block = (user_mem_block_t*)p;
    new_block->size = total_needed - sizeof(user_mem_block_t);
    new_block->is_free = 0;
    new_block->next = NULL;
    new_block->prev = NULL;

    if (!user_heap_head) {
        user_heap_head = new_block;
    } else {
        user_mem_block_t* last = user_heap_head;
        while (last->next) last = last->next;
        last->next = new_block;
        new_block->prev = last;
    }

    return (void*)((uint8_t*)new_block + sizeof(user_mem_block_t));
}

static inline void user_free(void* ptr) {
    if (!ptr) return;
    user_mem_block_t* block = (user_mem_block_t*)((uint8_t*)ptr - sizeof(user_mem_block_t));
    block->is_free = 1;
    if (block->next && block->next->is_free) {
        block->size += sizeof(user_mem_block_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
    }
    if (block->prev && block->prev->is_free) {
        block->prev->size += sizeof(user_mem_block_t) + block->size;
        block->prev->next = block->next;
        if (block->next) block->next->prev = block->prev;
    }
}

static inline void* user_calloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = user_malloc(total);
    if (ptr) {
        uint8_t* p = (uint8_t*)ptr;
        for (size_t i = 0; i < total; i++) p[i] = 0;
    }
    return ptr;
}

static inline void* user_realloc(void* ptr, size_t new_size) {
    if (!ptr) return user_malloc(new_size);
    if (new_size == 0) {
        user_free(ptr);
        return NULL;
    }
    user_mem_block_t* block = (user_mem_block_t*)((uint8_t*)ptr - sizeof(user_mem_block_t));
    if (block->size >= new_size) return ptr;

    void* new_ptr = user_malloc(new_size);
    if (new_ptr) {
        uint8_t* dst = (uint8_t*)new_ptr;
        uint8_t* src = (uint8_t*)ptr;
        size_t copy_bytes = (block->size < new_size) ? block->size : new_size;
        for (size_t i = 0; i < copy_bytes; i++) dst[i] = src[i];
        user_free(ptr);
    }
    return new_ptr;
}

// Userspace String & Formatting Utilities
static inline int user_strlen(const char* s) {
    int len = 0;
    while (s && s[len]) len++;
    return len;
}

static inline void user_puts(const char* str) {
    sys_write(1, str, user_strlen(str));
}

static inline void user_print_num(int n) {
    if (n == 0) {
        user_puts("0");
        return;
    }
    if (n < 0) {
        user_puts("-");
        n = -n;
    }
    char buf[16];
    int idx = 0;
    while (n > 0) {
        buf[idx++] = '0' + (n % 10);
        n /= 10;
    }
    for (int i = 0; i < idx / 2; i++) {
        char tmp = buf[i];
        buf[i] = buf[idx - 1 - i];
        buf[idx - 1 - i] = tmp;
    }
    buf[idx] = '\0';
    user_puts(buf);
}

#endif
