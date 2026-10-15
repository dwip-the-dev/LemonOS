#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdarg.h>
#include "syscall_nums.h"

int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    va_start(ap, request);
    unsigned int argp = va_arg(ap, unsigned int);
    va_end(ap);
    return __syscall_ret(__syscall3(SYS_IOCTL, (unsigned int)fd, (unsigned int)request, argp));
}

int tcgetattr(int fd, struct termios* termios_p) {
    if (!termios_p) {
        errno = EINVAL;
        return -1;
    }
    return ioctl(fd, 0x5401, termios_p); // TCGETS
}

int tcsetattr(int fd, int optional_actions, const struct termios* termios_p) {
    if (!termios_p) {
        errno = EINVAL;
        return -1;
    }
    unsigned long req = 0x5402; // TCSETS
    if (optional_actions == 1) req = 0x5403; // TCSETSW
    else if (optional_actions == 2) req = 0x5404; // TCSETSF
    return ioctl(fd, req, termios_p);
}

speed_t cfgetispeed(const struct termios* termios_p) {
    return termios_p ? termios_p->c_ispeed : 0;
}

speed_t cfgetospeed(const struct termios* termios_p) {
    return termios_p ? termios_p->c_ospeed : 0;
}

int cfsetispeed(struct termios* termios_p, speed_t speed) {
    if (termios_p) termios_p->c_ispeed = speed;
    return 0;
}

int cfsetospeed(struct termios* termios_p, speed_t speed) {
    if (termios_p) termios_p->c_ospeed = speed;
    return 0;
}

int tcflush(int fd, int queue_selector) {
    (void)queue_selector;
    return (fd >= 0) ? 0 : -1;
}
