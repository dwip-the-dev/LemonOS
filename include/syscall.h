#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

// Standard System Call Identifiers (POSIX / Linux i386)
#define SYS_EXIT         1
#define SYS_FORK         2
#define SYS_READ         3
#define SYS_WRITE        4
#define SYS_OPEN         5
#define SYS_CLOSE        6
#define SYS_WAITPID      7
#define SYS_CREAT        8
#define SYS_LINK         9
#define SYS_UNLINK       10
#define SYS_EXEC         11
#define SYS_EXECVE       11
#define SYS_CHDIR        12
#define SYS_TIME         13
#define SYS_MKNOD        14
#define SYS_CHMOD        15
#define SYS_STAT         18
#define SYS_LSEEK        19
#define SYS_GETPID       20
#define SYS_SETUID       23
#define SYS_GETUID       24
#define SYS_ALARM        27
#define SYS_FSTAT        28
#define SYS_PAUSE        29
#define SYS_ACCESS       33
#define SYS_SYNC         36
#define SYS_KILL         37
#define SYS_RENAME       38
#define SYS_MKDIR        39
#define SYS_RMDIR        40
#define SYS_DUP          41
#define SYS_PIPE         42
#define SYS_BRK          45
#define SYS_SETGID       46
#define SYS_GETGID       47
#define SYS_SIGNAL       48
#define SYS_GETEUID      49
#define SYS_GETEGID      50
#define SYS_IOCTL        54
#define SYS_FCNTL        55
#define SYS_DUP2         63
#define SYS_GETPPID      64
#define SYS_SIGACTION    67
#define SYS_SIGPROCMASK  72
#define SYS_GETTIMEOFDAY 78
#define SYS_MMAP         90
#define SYS_MUNMAP       91
#define SYS_GETDENTS     141
#define SYS_YIELD        158
#define SYS_NANOSLEEP    162
#define SYS_GETCWD       183

// LemonOS Custom & Multimedia Subsystems (200+)
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
#define SYS_SHMGET       210
#define SYS_SHMAT        211
#define SYS_SHMDT        212
#define SYS_SHMCTL       213
#define SYS_SEM_WAIT     214
#define SYS_SEM_POST     215
#define SYS_SEM_GETVAL   216

#define SYSCALL_MAX      256

// Register state captured upon interrupt / int 0x80
typedef struct {
    // Segment registers pushed manually
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    // General-purpose registers pushed by 'pusha'
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    // Interrupt number and error code
    uint32_t int_no;
    uint32_t err_code;

    // Automatically pushed by x86 CPU on privilege level transition
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t useresp;
    uint32_t ss;
} __attribute__((packed)) registers_t;

// Framebuffer Info structure for SYS_GFX_INFO
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
    uint32_t fb_address;
} gfx_info_t;

// System Call Subsystem API
void syscall_init(void);
int  syscall_dispatcher(registers_t* regs);
void syscall_print_stats(void);
void syscall_run_tests(void);

#endif
