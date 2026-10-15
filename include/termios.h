#ifndef TERMIOS_H
#define TERMIOS_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t tcflag_t;
typedef uint8_t  cc_t;
typedef uint32_t speed_t;

#define NCCS 32

// c_cc indices
#define VINTR    0
#define VQUIT    1
#define VERASE   2
#define VKILL    3
#define VEOF     4
#define VTIME    5
#define VMIN     6
#define VSWTC    7
#define VSTART   8
#define VSTOP    9
#define VSUSP    10
#define VEOL     11

// c_iflag bits
#define IGNBRK   0000001
#define BRKINT   0000002
#define IGNPAR   0000004
#define PARMRK   0000010
#define INPCK    0000020
#define ISTRIP   0000040
#define INLCR    0000100
#define IGNCR    0000200
#define ICRNL    0000400
#define IXON     0002000
#define IXOFF    0010000

// c_oflag bits
#define OPOST    0000001
#define ONLCR    0000004
#define OCRNL    0000010
#define ONOCR    0000020
#define ONLRET   0000040

// c_lflag bits
#define ISIG     0000001
#define ICANON   0000002
#define ECHO     0000010
#define ECHOE    0000020
#define ECHOK    0000040
#define ECHONL   0000100
#define NOFLSH   0000200
#define TOSTOP   0000400
#define IEXTEN   0010000

// tcsetattr actions
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t     c_line;
    cc_t     c_cc[NCCS];
    speed_t  c_ispeed;
    speed_t  c_ospeed;
};

struct winsize {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
};

void termios_init(void);
int  tcgetattr(int fd, struct termios* termios_p);
int  tcsetattr(int fd, int optional_actions, const struct termios* termios_p);
void cfmakeraw(struct termios* termios_p);
int  tcflush(int fd, int queue_selector);

// ANSI Escape sequence processor for terminal output
void termios_process_ansi(const char* input, void (*char_cb)(char c, uint8_t color), void (*cmd_cb)(int cmd, int arg1, int arg2));

// Verification tests
void termios_run_tests(void);

#endif
