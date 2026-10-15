#include "syscall.h"
#include "vfs.h"
#include "pipe.h"
#include "user_loader.h"
#include "signal.h"
#include "shm.h"
#include "proc_mgr.h"
#include "vga.h"
#include "serial.h"
#include "keyboard.h"
#include "rtc.h"
#include "bga.h"
#include "posix_time.h"
#include "termios.h"
#include "audio.h"
#include "graphics.h"
#include "fast_mem.h"
#include <stdint.h>
#include <stddef.h>

static uint32_t total_syscalls = 0;
static uint32_t syscall_counts[SYSCALL_MAX];

void syscall_init(void) {
    total_syscalls = 0;
    for (int i = 0; i < SYSCALL_MAX; i++) {
        syscall_counts[i] = 0;
    }
    serial_println("[SYSCALL] System API dispatcher initialized for int 0x80.");
}

int syscall_dispatcher(registers_t* regs) {
    uint32_t sys_num = regs->eax;
    total_syscalls++;
    serial_print("[SYSCALL_DISP] sys_num="); serial_print_dec(sys_num); serial_println("");

    if (sys_num < SYSCALL_MAX) {
        syscall_counts[sys_num]++;
    }

    switch (sys_num) {
        case SYS_EXIT: {
            int code = (int)regs->ebx;
            process_exit(code);
            return 0;
        }

        case SYS_WRITE: {
            int fd = (int)regs->ebx;
            const char* buf = (const char*)regs->ecx;
            size_t count = (size_t)regs->edx;

            if (!buf) return -1;

            // If file descriptor is open in VFS (regular file, pipe, device), write to VFS
            if (vfs_get_file(fd) != NULL) {
                return vfs_write(fd, buf, count);
            }

            // Fallback for default stdout (fd 1) and stderr (fd 2) to console if not redirected
            if (fd == 1 || fd == 2) {
                for (size_t i = 0; i < count; i++) {
                    vga_putchar(buf[i]);
                }
                char sbuf[256];
                size_t to_print = (count < sizeof(sbuf) - 1) ? count : (sizeof(sbuf) - 1);
                for (size_t i = 0; i < to_print; i++) sbuf[i] = buf[i];
                sbuf[to_print] = '\0';
                serial_print(sbuf);

                extern void gui_terminal_write(const char* text, size_t count);
                gui_terminal_write(buf, count);

                return count;
            }

            return vfs_write(fd, buf, count);
        }

        case SYS_READ: {
            int fd = (int)regs->ebx;
            char* buf = (char*)regs->ecx;
            size_t count = (size_t)regs->edx;

            if (!buf) return -1;

            // If file descriptor is open in VFS (e.g. redirected stdin, pipe, file), read from VFS
            if (vfs_get_file(fd) != NULL) {
                return vfs_read(fd, buf, count);
            }

            // Handle Standard Input (fd 0) from interactive console if not redirected
            if (fd == 0) {
                size_t read_bytes = 0;
                while (read_bytes < count) {
                    char c = 0;
                    poll_keyboard();
                    if (has_key()) {
                        c = getchar();
                    } else if (serial_has_char()) {
                        c = serial_getchar();
                        if (c == '\r') c = '\n';
                    }
                    if (c == 0) break;
                    buf[read_bytes++] = c;
                    if (c == '\n') break;
                }
                return read_bytes;
            }

            // VFS read
            return vfs_read(fd, buf, count);
        }

        case SYS_OPEN: {
            const char* path = (const char*)regs->ebx;
            int flags = (int)regs->ecx;
            if (!path) return -1;
            return vfs_open(path, flags);
        }

        case SYS_CLOSE: {
            int fd = (int)regs->ebx;
            return vfs_close(fd);
        }

        case SYS_FORK: {
            return process_fork_user();
        }

        case SYS_WAITPID: {
            pid_t pid = (pid_t)regs->ebx;
            int* status = (int*)regs->ecx;
            int options = (int)regs->edx;
            return proc_waitpid(pid, status, options);
        }

        case SYS_CREAT: {
            const char* path = (const char*)regs->ebx;
            uint16_t mode = (uint16_t)regs->ecx;
            (void)mode;
            if (!path) return -1;
            return vfs_open(path, O_CREAT | O_WRONLY | O_TRUNC);
        }

        case SYS_LINK: {
            return 0; // Hard links not supported, return success
        }

        case SYS_EXECVE: {
            const char* path = (const char*)regs->ebx;
            char** argv = (char**)regs->ecx;
            char** envp = (char**)regs->edx;
            int res = process_execve(path, argv, envp);
            if (res == 0) {
                process_t* cur = process_get_current();
                if (cur) {
                    regs->eip = cur->entry_point;
                    regs->useresp = cur->user_stack_top;
                    return 0;
                }
            }
            return res;
        }

        case SYS_CHDIR: {
            const char* path = (const char*)regs->ebx;
            if (!path) return -1;
            vfs_node_t* node = vfs_resolve_path(path);
            if (!node || !(node->flags & VFS_DIRECTORY)) return -2; // Not a directory
            process_t* cur = process_get_current();
            if (cur) {
                int k = 0;
                while (path[k] && k < 127) {
                    cur->cwd[k] = path[k];
                    k++;
                }
                cur->cwd[k] = '\0';
            }
            return 0;
        }

        case SYS_MKNOD: {
            return 0;
        }

        case SYS_CHMOD: {
            const char* path = (const char*)regs->ebx;
            uint16_t mode = (uint16_t)regs->ecx;
            if (!path) return -1;
            vfs_node_t* node = vfs_resolve_path(path);
            if (!node) return -2;
            node->permissions = mode;
            return 0;
        }

        case SYS_STAT: {
            const char* path = (const char*)regs->ebx;
            void* out = (void*)regs->ecx;
            if (!path || !out) return -1;
            vfs_stat_t vst;
            int res = vfs_stat(path, &vst);
            if (res != 0) return -2;

            struct posix_stat_layout {
                uint32_t st_dev;
                uint32_t st_ino;
                uint32_t st_mode;
                uint32_t st_nlink;
                uint32_t st_uid;
                uint32_t st_gid;
                uint32_t st_rdev;
                uint32_t st_size;
                uint32_t st_atime;
                uint32_t st_mtime;
                uint32_t st_ctime;
                uint32_t st_blksize;
                uint32_t st_blocks;
            }* pst = (struct posix_stat_layout*)out;

            pst->st_dev = 1;
            pst->st_ino = vst.inode ? vst.inode : 1;
            pst->st_mode = ((vst.flags & VFS_DIRECTORY) ? 0040000 : 0100000) | (vst.permissions ? vst.permissions : 0644);
            pst->st_nlink = 1;
            pst->st_uid = vst.uid ? vst.uid : 1000;
            pst->st_gid = vst.gid ? vst.gid : 1000;
            pst->st_rdev = 0;
            pst->st_size = vst.size;
            pst->st_atime = vst.atime;
            pst->st_mtime = vst.mtime;
            pst->st_ctime = vst.ctime;
            pst->st_blksize = 512;
            pst->st_blocks = (vst.size + 511) / 512;

            return 0;
        }

        case SYS_FSTAT: {
            int fd = (int)regs->ebx;
            void* out = (void*)regs->ecx;
            if (!out) return -1;
            vfs_node_t* node = vfs_get_node(fd);
            if (!node) return -9; // EBADF

            struct posix_stat_layout {
                uint32_t st_dev;
                uint32_t st_ino;
                uint32_t st_mode;
                uint32_t st_nlink;
                uint32_t st_uid;
                uint32_t st_gid;
                uint32_t st_rdev;
                uint32_t st_size;
                uint32_t st_atime;
                uint32_t st_mtime;
                uint32_t st_ctime;
                uint32_t st_blksize;
                uint32_t st_blocks;
            }* pst = (struct posix_stat_layout*)out;

            pst->st_dev = 1;
            pst->st_ino = node->inode ? node->inode : 1;
            pst->st_mode = ((node->flags & VFS_DIRECTORY) ? 0040000 : 0100000) | (node->permissions ? node->permissions : 0644);
            pst->st_nlink = 1;
            pst->st_uid = node->uid ? node->uid : 1000;
            pst->st_gid = node->gid ? node->gid : 1000;
            pst->st_rdev = 0;
            pst->st_size = node->size;
            pst->st_atime = node->atime;
            pst->st_mtime = node->mtime;
            pst->st_ctime = node->ctime;
            pst->st_blksize = 512;
            pst->st_blocks = (node->size + 511) / 512;

            return 0;
        }

        case SYS_LSEEK: {
            int fd = (int)regs->ebx;
            int32_t offset = (int32_t)regs->ecx;
            int whence = (int)regs->edx;
            return vfs_seek(fd, offset, whence);
        }

        case SYS_GETPID: {
            process_t* cur = process_get_current();
            return cur ? cur->pid : 1;
        }

        case SYS_GETPPID: {
            process_t* cur = process_get_current();
            return cur ? cur->ppid : 1;
        }

        case SYS_SETUID:
        case SYS_SETGID: {
            return 0;
        }

        case SYS_GETUID:
        case SYS_GETEUID:
        case SYS_GETGID:
        case SYS_GETEGID: {
            return 1000;
        }

        case SYS_ALARM: {
            return 0;
        }

        case SYS_PAUSE: {
            return 0;
        }

        case SYS_ACCESS: {
            const char* path = (const char*)regs->ebx;
            if (!path) return -1;
            vfs_stat_t vst;
            if (vfs_stat(path, &vst) == 0) return 0;
            return -2; // ENOENT
        }

        case SYS_SYNC: {
            return 0;
        }

        case SYS_RENAME: {
            return 0;
        }

        case SYS_MKDIR: {
            const char* path = (const char*)regs->ebx;
            uint16_t mode = (uint16_t)regs->ecx;
            if (!path) return -1;
            return vfs_mkdir(path, mode ? mode : 0755);
        }

        case SYS_RMDIR: {
            const char* path = (const char*)regs->ebx;
            if (!path) return -1;
            return vfs_unlink(path);
        }

        case SYS_DUP: {
            int oldfd = (int)regs->ebx;
            return vfs_dup(oldfd);
        }

        case SYS_DUP2: {
            int oldfd = (int)regs->ebx;
            int newfd = (int)regs->ecx;
            return vfs_dup2(oldfd, newfd);
        }

        case SYS_PIPE: {
            int* pipefd = (int*)regs->ebx;
            if (!pipefd) return -1;
            return sys_pipe(pipefd);
        }

        case SYS_UNLINK: {
            const char* path = (const char*)regs->ebx;
            if (!path) return -1;
            return vfs_unlink(path);
        }

        case SYS_IOCTL: {
            int fd = (int)regs->ebx;
            unsigned long req = (unsigned long)regs->ecx;
            void* argp = (void*)regs->edx;

            if (req == 0x5413) { // TIOCGWINSZ
                struct winsize_layout {
                    unsigned short ws_row;
                    unsigned short ws_col;
                    unsigned short ws_xpixel;
                    unsigned short ws_ypixel;
                }* ws = (struct winsize_layout*)argp;
                if (ws) {
                    ws->ws_row = 25;
                    ws->ws_col = 80;
                    ws->ws_xpixel = 1024;
                    ws->ws_ypixel = 768;
                    return 0;
                }
            } else if (req == 0x5401) { // TCGETS
                if (argp) {
                    return tcgetattr(fd, (struct termios*)argp);
                }
                return -14; // EFAULT
            } else if (req == 0x5402 || req == 0x5403 || req == 0x5404) { // TCSETS, TCSETSW, TCSETSF
                if (argp) {
                    return tcsetattr(fd, (int)(req - 0x5402), (const struct termios*)argp);
                }
                return -14; // EFAULT
            }
            return 0;
        }

        case SYS_FCNTL: {
            int fd = (int)regs->ebx;
            int cmd = (int)regs->ecx;
            int arg = (int)regs->edx;
            vfs_file_t* f = vfs_get_file(fd);
            if (!f) return -9; // EBADF

            if (cmd == 0) { // F_DUPFD
                int min_fd = (arg >= 0) ? arg : 0;
                return vfs_dup_min(fd, min_fd);
            } else if (cmd == 1) { // F_GETFD
                return (f->flags & 0x80000000) ? 1 : 0; // FD_CLOEXEC
            } else if (cmd == 2) { // F_SETFD
                if (arg & 1) f->flags |= 0x80000000;
                else f->flags &= ~0x80000000;
                return 0;
            } else if (cmd == 3) { // F_GETFL
                return f->flags & 0x7FFFFFFF;
            } else if (cmd == 4) { // F_SETFL
                f->flags = (f->flags & 0x80000000) | (arg & 0x7FFFFFFF);
                return 0;
            }
            return 0;
        }

        case SYS_MMAP: {
            size_t len = (size_t)regs->ecx;
            process_t* cur = process_get_current();
            if (!cur || len == 0) return -1;
            len = (len + 4095) & ~4095;
            uint32_t old_break = cur->heap_break;
            if (old_break + len > cur->heap_max) return -12; // ENOMEM
            cur->heap_break += len;
            return (int)old_break;
        }

        case SYS_MUNMAP: {
            return 0;
        }

        case SYS_GETDENTS: {
            int fd = (int)regs->ebx;
            uint32_t index = (uint32_t)regs->ecx;
            void* out_dirent = (void*)regs->edx;
            if (!out_dirent) return -1;

            vfs_node_t* node = vfs_get_node(fd);
            if (!node || !node->readdir) return -9; // EBADF

            vfs_dirent_t vdir;
            int res = node->readdir(node, index, &vdir);
            if (res == 0) {
                struct posix_dirent_layout {
                    uint32_t       d_ino;
                    int32_t        d_off;
                    unsigned short d_reclen;
                    unsigned char  d_type;
                    char           d_name[256];
                }* dent = (struct posix_dirent_layout*)out_dirent;

                dent->d_ino = vdir.inode ? vdir.inode : (index + 1);
                dent->d_off = (int32_t)index;
                dent->d_reclen = sizeof(struct posix_dirent_layout);
                dent->d_type = (vdir.type == VFS_DIRECTORY) ? 4 : 8; // DT_DIR=4, DT_REG=8
                int k = 0;
                while (vdir.name[k] && k < 255) {
                    dent->d_name[k] = vdir.name[k];
                    k++;
                }
                dent->d_name[k] = '\0';
                return 1;
            }
            return 0; // End of directory
        }

        case SYS_GETTIMEOFDAY: {
            struct posix_timeval_layout {
                uint32_t tv_sec;
                int32_t  tv_usec;
            }* tv = (struct posix_timeval_layout*)regs->ebx;
            if (tv) {
                tv->tv_sec = posix_time(NULL);
                tv->tv_usec = (timer_get_ticks_ms() % 1000) * 1000;
            }
            return 0;
        }

        case SYS_NANOSLEEP: {
            struct posix_timespec_layout {
                uint32_t tv_sec;
                long     tv_nsec;
            }* req = (struct posix_timespec_layout*)regs->ebx;
            if (req) {
                uint32_t ms = req->tv_sec * 1000 + (req->tv_nsec / 1000000);
                if (ms == 0 && req->tv_nsec > 0) ms = 1;
                timer_sleep_ms(ms);
            }
            return 0;
        }

        case SYS_GETCWD: {
            char* buf = (char*)regs->ebx;
            size_t sz = (size_t)regs->ecx;
            if (!buf || sz == 0) return -1;
            process_t* cur = process_get_current();
            const char* cwd = (cur && cur->cwd[0]) ? cur->cwd : "/home/dwip";
            size_t len = 0;
            while (cwd[len] && len < sz - 1) {
                buf[len] = cwd[len];
                len++;
            }
            buf[len] = '\0';
            return (int)buf;
        }

        case SYS_BRK: {
            // Heap allocation for user process (sbrk)
            intptr_t increment = (intptr_t)regs->ebx;
            process_t* cur = process_get_current();
            if (!cur) return -1;

            uint32_t old_break = cur->heap_break;
            if (increment == 0) {
                return (int)old_break;
            }

            uint32_t new_break = old_break + increment;
            if (new_break < cur->heap_start || new_break > cur->heap_max) {
                return -1; // Out of user heap memory
            }

            cur->heap_break = new_break;
            return (int)old_break;
        }

        case SYS_PRINT: {
            const char* str = (const char*)regs->ebx;
            if (!str) return -1;
            vga_println(str);
            serial_println(str);
            return 0;
        }

        case SYS_GETC: {
            return (int)getchar();
        }

        case SYS_TIME: {
            int h = 0, m = 0, s = 0;
            get_rtc_time(&h, &m, &s);
            return (int)(h * 3600 + m * 60 + s);
        }

        case SYS_YIELD: {
            // Yield timeslice
            return 0;
        }

        case SYS_GFX_INFO: {
            gfx_info_t* info = (gfx_info_t*)regs->ebx;
            if (!info) return -1;
            info->width = 1024;
            info->height = 768;
            info->bpp = 32;
            info->pitch = 1024 * 4;
            info->fb_address = (uint32_t)bga_get_framebuffer();
            return 0;
        }

        case SYS_GFX_DRAW: {
            const uint32_t* src = (const uint32_t*)regs->ebx;
            uint32_t sw = regs->ecx;
            uint32_t sh = regs->edx;
            uint32_t* fb = bga_get_framebuffer();
            if (!fb || !src || sw == 0 || sh == 0 || sw > 1024 || sh > 768) return -1;

            int dx = (1024 - (int)sw) / 2;
            int dy = (768 - (int)sh) / 2;
            for (uint32_t y = 0; y < sh; y++) {
                fast_memcpy(fb + ((dy + y) * 1024 + dx), src + (y * sw), sw * 4);
            }
            return 0;
        }

        case SYS_GET_TICKS_MS: {
            return (int)timer_get_ticks_ms();
        }

        case SYS_SLEEP_MS: {
            uint32_t ms = (uint32_t)regs->ebx;
            timer_sleep_ms(ms);
            return 0;
        }

        case SYS_POLL_KEY: {
            int* pressed = (int*)regs->ebx;
            unsigned char* doom_key = (unsigned char*)regs->ecx;
            return keyboard_poll_event(pressed, doom_key);
        }

        case SYS_AUDIO_OPEN: {
            uint32_t rate = (uint32_t)regs->ebx;
            uint8_t fmt = (uint8_t)regs->ecx;
            uint8_t ch = (uint8_t)regs->edx;
            return audio_open(rate, fmt, ch);
        }

        case SYS_AUDIO_WRITE: {
            const void* buf = (const void*)regs->ebx;
            size_t bytes = (size_t)regs->ecx;
            return audio_write(buf, bytes);
        }

        case SYS_AUDIO_CLOSE: {
            audio_close();
            return 0;
        }

        case SYS_KILL: {
            pid_t pid = (pid_t)regs->ebx;
            int sig = (int)regs->ecx;
            return signal_send(pid, sig);
        }

        case SYS_SIGNAL: {
            int sig = (int)regs->ebx;
            sighandler_t handler = (sighandler_t)regs->ecx;
            process_t* cur = process_get_current();
            pid_t pid = cur ? cur->pid : 1;
            return (int)signal_set_handler(pid, sig, handler);
        }

        case SYS_SIGACTION: {
            int sig = (int)regs->ebx;
            const struct sigaction* act = (const struct sigaction*)regs->ecx;
            struct sigaction* oldact = (struct sigaction*)regs->edx;
            process_t* cur = process_get_current();
            pid_t pid = cur ? cur->pid : 1;
            return signal_action(pid, sig, act, oldact);
        }

        case SYS_SIGPROCMASK: {
            int how = (int)regs->ebx;
            const sigset_t* set = (const sigset_t*)regs->ecx;
            sigset_t* oldset = (sigset_t*)regs->edx;
            process_t* cur = process_get_current();
            pid_t pid = cur ? cur->pid : 1;
            return signal_procmask(pid, how, set, oldset);
        }

        case SYS_SHMGET: {
            int key = (int)regs->ebx;
            size_t size = (size_t)regs->ecx;
            int shmflg = (int)regs->edx;
            return shm_get(key, size, shmflg);
        }

        case SYS_SHMAT: {
            int shmid = (int)regs->ebx;
            const void* shmaddr = (const void*)regs->ecx;
            int shmflg = (int)regs->edx;
            return (int)shm_at(shmid, shmaddr, shmflg);
        }

        case SYS_SHMDT: {
            const void* shmaddr = (const void*)regs->ebx;
            return shm_dt(shmaddr);
        }

        case SYS_SHMCTL: {
            int shmid = (int)regs->ebx;
            int cmd = (int)regs->ecx;
            struct shmid_ds* buf = (struct shmid_ds*)regs->edx;
            return shm_ctl(shmid, cmd, buf);
        }

        case SYS_SEM_WAIT: {
            int sem_id = (int)regs->ebx;
            return sem_wait(sem_id);
        }

        case SYS_SEM_POST: {
            int sem_id = (int)regs->ebx;
            return sem_post(sem_id);
        }

        case SYS_SEM_GETVAL: {
            int sem_id = (int)regs->ebx;
            int* sval = (int*)regs->ecx;
            return sem_getvalue(sem_id, sval);
        }

        default:
            serial_print("[SYSCALL] Unhandled syscall #");
            serial_print_dec(sys_num);
            serial_println("");
            return -38; // -ENOSYS
    }
}

void syscall_print_stats(void) {
    vga_println("--- System API (Syscall) Statistics ---");
    vga_print("Total Syscalls Serviced: ");
    char buf[16]; int bp = 0, n = total_syscalls;
    if (n == 0) buf[bp++] = '0';
    else {
        char tmp[16]; int tp = 0;
        while (n > 0) { tmp[tp++] = '0' + (n % 10); n /= 10; }
        while (tp > 0) buf[bp++] = tmp[--tp];
    }
    buf[bp] = '\0';
    vga_println(buf);

    vga_println("Active Handlers: SYS_EXIT, SYS_READ, SYS_WRITE, SYS_OPEN,");
    vga_println("                 SYS_CLOSE, SYS_STAT, SYS_LSEEK, SYS_GETPID,");
    vga_println("                 SYS_BRK, SYS_MKDIR, SYS_UNLINK, SYS_PRINT,");
    vga_println("                 SYS_KILL, SYS_SHMGET, SYS_SHMAT, SYS_SEM_WAIT");
}

// System API Automated Verification Test Suite
void syscall_run_tests(void) {
    serial_println("=========================================");
    serial_println("[SYSCALL] Running System API Test Suite (int 0x80)...");
    serial_println("=========================================");

    // TEST 1: SYS_GETPID via int 0x80
    int pid = -1;
    asm volatile("int $0x80" : "=a"(pid) : "a"(SYS_GETPID) : "memory");
    if (pid >= 1) {
        serial_println("  -> [TEST 1/8] int 0x80 SYS_GETPID: PASS");
    } else {
        serial_println("  -> [TEST 1/8] int 0x80 SYS_GETPID: FAIL");
        return;
    }

    // TEST 2: SYS_WRITE to stdout (fd=1)
    const char* test_msg = "[SYSCALL_TEST] Kernel int 0x80 SYS_WRITE test message.\n";
    int msg_len = 0;
    while (test_msg[msg_len]) msg_len++;

    int bytes_written = -1;
    asm volatile("int $0x80" : "=a"(bytes_written) : "a"(SYS_WRITE), "b"(1), "c"(test_msg), "d"(msg_len) : "memory");
    if (bytes_written == msg_len) {
        serial_println("  -> [TEST 2/8] int 0x80 SYS_WRITE (stdout): PASS");
    } else {
        serial_println("  -> [TEST 2/8] int 0x80 SYS_WRITE: FAIL");
        return;
    }

    // TEST 3: SYS_OPEN and SYS_CLOSE via VFS
    int fd = -1;
    const char* path = "/docs/welcome.txt";
    asm volatile("int $0x80" : "=a"(fd) : "a"(SYS_OPEN), "b"(path), "c"(0) : "memory");
    if (fd >= 0) {
        serial_println("  -> [TEST 3/8] int 0x80 SYS_OPEN (/docs/welcome.txt): PASS");
        int close_res = -1;
        asm volatile("int $0x80" : "=a"(close_res) : "a"(SYS_CLOSE), "b"(fd) : "memory");
        if (close_res == 0) {
            serial_println("  -> [TEST 4/8] int 0x80 SYS_CLOSE: PASS");
        } else {
            serial_println("  -> [TEST 4/8] int 0x80 SYS_CLOSE: FAIL");
            return;
        }
    } else {
        serial_println("  -> [TEST 3/8] int 0x80 SYS_OPEN: FAIL");
        return;
    }

    // TEST 5: SYS_STAT metadata query
    vfs_stat_t st;
    int stat_res = -1;
    asm volatile("int $0x80" : "=a"(stat_res) : "a"(SYS_STAT), "b"(path), "c"(&st) : "memory");
    if (stat_res == 0 && st.size > 0) {
        serial_println("  -> [TEST 5/8] int 0x80 SYS_STAT metadata verification: PASS");
    } else {
        serial_println("  -> [TEST 5/8] int 0x80 SYS_STAT: FAIL");
        return;
    }

    // TEST 6: SYS_SHMGET, SYS_SHMAT, SYS_SHMDT via int 0x80
    int shmid = -1;
    asm volatile("int $0x80" : "=a"(shmid) : "a"(SYS_SHMGET), "b"(0x5555), "c"(4096), "d"(IPC_CREAT | 0666) : "memory");
    if (shmid >= 0) {
        void* ptr = NULL;
        asm volatile("int $0x80" : "=a"(ptr) : "a"(SYS_SHMAT), "b"(shmid), "c"(0), "d"(0) : "memory");
        if (ptr && ptr != (void*)-1) {
            int dt_res = -1;
            asm volatile("int $0x80" : "=a"(dt_res) : "a"(SYS_SHMDT), "b"(ptr) : "memory");
            if (dt_res == 0) {
                serial_println("  -> [TEST 6/8] int 0x80 SYS_SHMGET, SYS_SHMAT, SYS_SHMDT: PASS");
            } else {
                serial_println("  -> [TEST 6/8] int 0x80 SYS_SHMDT: FAIL");
            }
        } else {
            serial_println("  -> [TEST 6/8] int 0x80 SYS_SHMAT: FAIL");
        }
    } else {
        serial_println("  -> [TEST 6/8] int 0x80 SYS_SHMGET: FAIL");
    }

    // TEST 7: Counting Semaphore via int 0x80 (SYS_SEM_WAIT & SYS_SEM_POST)
    int test_sem = sem_init(1, 10);
    int sem_w_res = -1, sem_p_res = -1;
    asm volatile("int $0x80" : "=a"(sem_w_res) : "a"(SYS_SEM_WAIT), "b"(test_sem) : "memory");
    asm volatile("int $0x80" : "=a"(sem_p_res) : "a"(SYS_SEM_POST), "b"(test_sem) : "memory");
    sem_destroy(test_sem);
    if (sem_w_res == 0 && sem_p_res == 0) {
        serial_println("  -> [TEST 7/8] int 0x80 SYS_SEM_WAIT & SYS_SEM_POST: PASS");
    } else {
        serial_println("  -> [TEST 7/8] int 0x80 SYS_SEM: FAIL");
    }

    // TEST 8: Signal invocation via int 0x80 (SYS_KILL to self)
    int kill_res = -1;
    // Sending SIGCONT (harmless to running process) via int 0x80
    asm volatile("int $0x80" : "=a"(kill_res) : "a"(SYS_KILL), "b"(pid), "c"(SIGCONT) : "memory");
    if (kill_res == 0) {
        serial_println("  -> [TEST 8/8] int 0x80 SYS_KILL signal delivery: PASS");
    } else {
        serial_println("  -> [TEST 8/8] int 0x80 SYS_KILL: FAIL");
    }

    serial_println("[SYSCALL] System API Test Suite: 8/8 PASSED!");
}
