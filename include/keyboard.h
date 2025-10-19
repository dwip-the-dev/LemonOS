#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
void poll_keyboard(void);
char getchar(void);
int has_key(void);
int is_shift_pressed(void);
int is_caps_lock_on(void);
int is_ctrl_pressed(void);  // Add these
int is_alt_pressed(void);

#endif
