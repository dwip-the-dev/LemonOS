#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

#define MOUSE_BTN_LEFT   (1 << 0)
#define MOUSE_BTN_RIGHT  (1 << 1)
#define MOUSE_BTN_MIDDLE (1 << 2)

void mouse_init(int max_x, int max_y);
void mouse_poll(void);
int  mouse_is_enabled(void);

int mouse_get_x(void);
int mouse_get_y(void);
int mouse_get_buttons(void);
int mouse_get_delta(int* dx, int* dy);

int mouse_is_left_pressed(void);
int mouse_is_right_pressed(void);
int mouse_is_middle_pressed(void);

int mouse_is_left_clicked(void);
int mouse_is_right_clicked(void);
int mouse_is_left_released(void);

int mouse_get_scroll_delta(void);
int mouse_has_scroll_wheel(void);

void mouse_set_bounds(int max_x, int max_y);
void mouse_set_position(int x, int y);

#endif
