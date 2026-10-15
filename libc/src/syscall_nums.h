#ifndef _LIBC_SYSCALL_NUMS_H
#define _LIBC_SYSCALL_NUMS_H

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
#define SYS_NANOSLEEP    162
#define SYS_GETCWD       183

extern int __syscall0(int num);
extern int __syscall1(int num, unsigned int a1);
extern int __syscall2(int num, unsigned int a1, unsigned int a2);
extern int __syscall3(int num, unsigned int a1, unsigned int a2, unsigned int a3);
extern int __syscall4(int num, unsigned int a1, unsigned int a2, unsigned int a3, unsigned int a4);
extern int __syscall5(int num, unsigned int a1, unsigned int a2, unsigned int a3, unsigned int a4, unsigned int a5);
extern int __syscall_ret(int ret);

#endif // _LIBC_SYSCALL_NUMS_H
