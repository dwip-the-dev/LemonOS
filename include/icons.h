#ifndef ICONS_H
#define ICONS_H

#include <stdint.h>

// Icon identifiers (vector & pixel assets)
typedef enum {
    ICON_NONE = 0,
    ICON_LEMON_OS = 1,
    ICON_LEMON = 1,
    ICON_TERMINAL = 2,
    ICON_CALCULATOR = 3,
    ICON_FILE_MANAGER = 4,
    ICON_FILES = 4,
    ICON_SYSINFO = 5,
    ICON_INFO = 5,
    ICON_CLOCK = 6,
    ICON_GAMES = 7,
    ICON_SETTINGS = 8,
    ICON_POWER = 9,
    ICON_CLOSE = 10,
    ICON_MINIMIZE = 11,
    ICON_MAXIMIZE = 12,
    ICON_SPEAKER = 13,
    ICON_SOUND = 13,
    ICON_BATTERY = 14,
    ICON_WIFI = 15,
    ICON_CONVERTER = 16,
    ICON_CONVERT = 16,
    ICON_MATRIX = 17,
    ICON_RESTART = 18,
    ICON_SEARCH = 19,
    ICON_RESTORE = 20,
    ICON_FIREFOX = 21,
    ICON_GLOBE = 22,
    ICON_LOCK = 23,
    ICON_HOME = 24,
    ICON_REFRESH = 25,
    ICON_ARROW_LEFT = 26,
    ICON_ARROW_RIGHT = 27
} icon_type_t;

// Render icon of given size (24x24 or 32x32 or 16x16)
void icon_draw_16(int x, int y, icon_type_t type, uint32_t color);
void icon_draw_24(int x, int y, icon_type_t type);
void icon_draw_32(int x, int y, icon_type_t type);

#endif
