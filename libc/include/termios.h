#ifndef _TERMIOS_H
#define _TERMIOS_H

typedef unsigned char  cc_t;
typedef unsigned int   speed_t;
typedef unsigned int   tcflag_t;

#define NCCS 32

struct termios {
    tcflag_t c_iflag;      /* input modes */
    tcflag_t c_oflag;      /* output modes */
    tcflag_t c_cflag;      /* control modes */
    tcflag_t c_lflag;      /* local modes */
    cc_t     c_line;       /* line discipline */
    cc_t     c_cc[NCCS];   /* control characters */
    speed_t  c_ispeed;     /* input baud rate */
    speed_t  c_ospeed;     /* output baud rate */
};

/* c_lflag bits */
#define ISIG    0000001
#define ICANON  0000002
#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100
#define NOFLSH  0000200
#define TOSTOP  0000400
#define IEXTEN  0100000

/* tcsetattr actions */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

/* tcflush actions */
#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2

int     tcgetattr(int fd, struct termios* termios_p);
int     tcsetattr(int fd, int optional_actions, const struct termios* termios_p);
speed_t cfgetispeed(const struct termios* termios_p);
speed_t cfgetospeed(const struct termios* termios_p);
int     cfsetispeed(struct termios* termios_p, speed_t speed);
int     cfsetospeed(struct termios* termios_p, speed_t speed);
int     tcflush(int fd, int queue_selector);

#endif // _TERMIOS_H
