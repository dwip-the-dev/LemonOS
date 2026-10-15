#ifndef FONT_H
#define FONT_H

#include <stdint.h>

#include "icons.h"

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

void font_draw_char(uint32_t* buffer, int buf_w, int buf_h, int x, int y, char c, uint32_t fg, uint32_t bg, int transparent);
void font_draw_char_scaled(uint32_t* buffer, int buf_w, int buf_h, int x, int y, char c, uint32_t fg, int scale);
void font_draw_string(uint32_t* buffer, int buf_w, int buf_h, int x, int y, const char* str, uint32_t fg, uint32_t bg, int transparent);
void font_draw_string_scaled(uint32_t* buffer, int buf_w, int buf_h, int x, int y, const char* str, uint32_t fg, int scale);
void font_draw_string_clipped(uint32_t* buffer, int buf_w, int buf_h, int x, int y, int max_w, const char* str, uint32_t fg, uint32_t bg, int transparent);
void font_draw_string_ellipsis(uint32_t* buffer, int buf_w, int buf_h, int x, int y, int max_w, const char* str, uint32_t fg, uint32_t bg, int transparent);
int  font_get_string_width(const char* str);
void font_draw_icon(uint32_t* buffer, int buf_w, int buf_h, int x, int y, int icon_id, uint32_t color);

#endif
