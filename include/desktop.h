#ifndef DESKTOP_H
#define DESKTOP_H

#include <stdint.h>

#define THEME_LEMON_GOLD 0
#define THEME_GNOME_DARK 1
#define THEME_WIN_BLOOM  2
#define THEME_EMERALD    3

void desktop_init(void);
void desktop_run(void);
void desktop_exit(void);

void desktop_set_theme(int theme_id);
int  desktop_get_theme(void);

// Custom Wallpaper Management
int  desktop_set_wallpaper(const char* filepath);
const char* desktop_get_wallpaper(void);
void desktop_reset_wallpaper(void);
int  desktop_has_custom_wallpaper(void);

// Launchers for built-in apps
void app_firefox_launch(void);
void app_terminal_launch(void);
void app_calculator_launch(void);
void app_unit_converter_launch(void);
void app_system_info_launch(void);
void app_file_manager_launch(void);
void app_clock_launch(void);
void app_snake_launch(void);
void app_pong_launch(void);
void app_breakout_launch(void);
void app_matrix_launch(void);
void app_settings_launch(void);
void app_doom_launch(void);

#endif
