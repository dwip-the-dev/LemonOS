#include "termios.h"
#include "string.h"
#include "serial.h"

static struct termios default_term;

void termios_init(void) {
    memset(&default_term, 0, sizeof(default_term));
    default_term.c_iflag = ICRNL | IXON;
    default_term.c_oflag = OPOST | ONLCR;
    default_term.c_cflag = 0;
    default_term.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;

    default_term.c_cc[VINTR]  = 0x03; // Ctrl-C
    default_term.c_cc[VQUIT]  = 0x1C; // Ctrl-backslash
    default_term.c_cc[VERASE] = 0x7F; // Backspace
    default_term.c_cc[VKILL]  = 0x15; // Ctrl-U
    default_term.c_cc[VEOF]   = 0x04; // Ctrl-D
    default_term.c_cc[VTIME]  = 0;
    default_term.c_cc[VMIN]   = 1;

    serial_println("[TERMIOS] POSIX terminal control subsystem initialized.");
}

int tcgetattr(int fd, struct termios* termios_p) {
    (void)fd;
    if (!termios_p) return -1;
    *termios_p = default_term;
    return 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios* termios_p) {
    (void)fd;
    (void)optional_actions;
    if (!termios_p) return -1;
    default_term = *termios_p;
    return 0;
}

void cfmakeraw(struct termios* termios_p) {
    if (!termios_p) return;
    termios_p->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    termios_p->c_oflag &= ~OPOST;
    termios_p->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    termios_p->c_cflag &= 0;
    termios_p->c_cc[VMIN] = 1;
    termios_p->c_cc[VTIME] = 0;
}

int tcflush(int fd, int queue_selector) {
    (void)fd;
    (void)queue_selector;
    return 0;
}

void termios_process_ansi(const char* input, void (*char_cb)(char c, uint8_t color), void (*cmd_cb)(int cmd, int arg1, int arg2)) {
    if (!input) return;

    uint8_t cur_color = 0x07; // Light gray on black
    int state = 0; // 0=normal, 1=ESC seen, 2=CSI seen '['
    int num1 = 0, num2 = 0;
    int have_semicolon = 0;

    for (size_t i = 0; input[i] != '\0'; i++) {
        char c = input[i];
        if (state == 0) {
            if (c == '\033') {
                state = 1;
            } else {
                if (char_cb) char_cb(c, cur_color);
            }
        } else if (state == 1) {
            if (c == '[') {
                state = 2;
                num1 = 0;
                num2 = 0;
                have_semicolon = 0;
            } else {
                state = 0;
                if (char_cb) {
                    char_cb('\033', cur_color);
                    char_cb(c, cur_color);
                }
            }
        } else if (state == 2) {
            if (c >= '0' && c <= '9') {
                if (!have_semicolon) {
                    num1 = num1 * 10 + (c - '0');
                } else {
                    num2 = num2 * 10 + (c - '0');
                }
            } else if (c == ';') {
                have_semicolon = 1;
            } else if (c == 'm') { // Color attribute command
                if (num1 == 0) cur_color = 0x07; // Reset
                else if (num1 == 31) cur_color = 0x04; // Red
                else if (num1 == 32) cur_color = 0x02; // Green
                else if (num1 == 33) cur_color = 0x0E; // Yellow
                else if (num1 == 34) cur_color = 0x01; // Blue
                else if (num1 == 35) cur_color = 0x05; // Magenta
                else if (num1 == 36) cur_color = 0x03; // Cyan
                else if (num1 == 37) cur_color = 0x0F; // White
                state = 0;
            } else if (c == 'H' || c == 'f') { // Cursor position
                if (cmd_cb) cmd_cb('H', num1, num2);
                state = 0;
            } else if (c == 'J') { // Clear screen
                if (cmd_cb) cmd_cb('J', num1, 0);
                state = 0;
            } else if (c == 'K') { // Clear line
                if (cmd_cb) cmd_cb('K', num1, 0);
                state = 0;
            } else {
                state = 0;
            }
        }
    }
}

void termios_run_tests(void) {
    serial_println("[TERMIOS] Running POSIX Terminal Verification Tests...");
    struct termios t;
    if (tcgetattr(0, &t) == 0 && (t.c_lflag & ICANON) && (t.c_lflag & ECHO)) {
        serial_println("  -> [TEST 1/2] tcgetattr canonical echo mode: PASS");
    } else {
        serial_println("  -> [TEST 1/2] tcgetattr canonical echo: FAIL");
    }

    cfmakeraw(&t);
    if (!(t.c_lflag & ICANON) && !(t.c_lflag & ECHO)) {
        serial_println("  -> [TEST 2/2] cfmakeraw non-canonical raw mode: PASS");
    } else {
        serial_println("  -> [TEST 2/2] cfmakeraw: FAIL");
    }
    serial_println("[TERMIOS] Terminal control subsystem verified.");
}
