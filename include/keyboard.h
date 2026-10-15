#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
void poll_keyboard(void);
char getchar(void);
int has_key(void);
int is_shift_pressed(void);
int is_caps_lock_on(void);
int is_ctrl_pressed(void);
int is_alt_pressed(void);
int check_and_clear_alt_f4(void);

// Special characters for function keys and arrows
#define CHAR_F1     0x01
#define CHAR_F2     0x02  
#define CHAR_F3     0x03
#define CHAR_F4     0x04
#define CHAR_F5     0x05
#define CHAR_F6     0x06
#define CHAR_F7     0x07
#define CHAR_F8     0x08
#define CHAR_F9     0x09
#define CHAR_F10    0x0A
#define CHAR_F11    0x0B
#define CHAR_F12    0x0C
#define CHAR_UP     0x11
#define CHAR_DOWN   0x12
#define CHAR_LEFT   0x13
#define CHAR_RIGHT  0x14
#define CHAR_HOME     0x15
#define CHAR_END      0x16
#define CHAR_DELETE   0x17
#define CHAR_INSERT   0x18
#define CHAR_PAGEUP   0x19
#define CHAR_PAGEDOWN 0x1A

// Non-blocking Key Event API for Games & Input Handlers
int keyboard_poll_event(int* pressed, unsigned char* doom_key);
void keyboard_clear_events(void);

#endif
