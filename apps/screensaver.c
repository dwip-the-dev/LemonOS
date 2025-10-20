#include "screensaver.h"
#include "vga.h"
#include "keyboard.h"
#include <stdint.h>

#define MATRIX_CHARS " 01abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()_+-=[]{}|;:,.<>?/~"
#define MATRIX_CHARS_LEN 85
#define MAX_DROPS 100
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25

typedef struct {
    int x, y;
    int speed;
    int length;
    int brightness;
    int active;
    char chars[30];
    int head_char;
} matrix_drop;

static int screen_saver_active = 0;
static unsigned long last_activity = 0;
static unsigned long tick_counter = 0;
static matrix_drop drops[MAX_DROPS];
static uint8_t screen_buffer[SCREEN_HEIGHT][SCREEN_WIDTH];

unsigned int simple_rand(void) {
    static unsigned int seed = 12345;
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
    return seed;
}

void update_tick(void) {
    tick_counter++;
}

void update_activity_time(void) {
    last_activity = tick_counter;
    screen_saver_active = 0;
}

int should_activate_screensaver(void) {
    unsigned long timeout = 3000000; // ~30 seconds
    return ((tick_counter - last_activity) > timeout) && !screen_saver_active;
}

void init_matrix_drops(void) {
    for (int i = 0; i < MAX_DROPS; i++) {
        drops[i].x = simple_rand() % SCREEN_WIDTH;
        drops[i].y = -(simple_rand() % 20);
        drops[i].speed = 1 + (simple_rand() % 3);
        drops[i].length = 5 + (simple_rand() % 20);
        drops[i].brightness = 255;
        drops[i].active = 1;
        drops[i].head_char = simple_rand() % MATRIX_CHARS_LEN;
        for (int j = 0; j < drops[i].length; j++)
            drops[i].chars[j] = MATRIX_CHARS[simple_rand() % MATRIX_CHARS_LEN];
    }
    for (int y = 0; y < SCREEN_HEIGHT; y++)
        for (int x = 0; x < SCREEN_WIDTH; x++)
            screen_buffer[y][x] = ' ';
}

void update_matrix_drops(void) {
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!drops[i].active) {
            if ((simple_rand() % 100) == 0) {
                drops[i].x = simple_rand() % SCREEN_WIDTH;
                drops[i].y = -(simple_rand() % 10);
                drops[i].speed = 1 + (simple_rand() % 3);
                drops[i].length = 5 + (simple_rand() % 20);
                drops[i].brightness = 255;
                drops[i].active = 1;
                drops[i].head_char = simple_rand() % MATRIX_CHARS_LEN;
            }
            continue;
        }
        drops[i].y += drops[i].speed;
        if ((tick_counter % (5 + i)) == 0)
            drops[i].head_char = simple_rand() % MATRIX_CHARS_LEN;
        if ((simple_rand() % 20) == 0)
            drops[i].chars[simple_rand() % drops[i].length] = MATRIX_CHARS[simple_rand() % MATRIX_CHARS_LEN];
        if (drops[i].y > SCREEN_HEIGHT + drops[i].length) {
            drops[i].brightness -= 10;
            if (drops[i].brightness <= 0) drops[i].active = 0;
        }
    }
}

void add_sparkles(void) {
    for (int i = 0; i < 5; i++) {
        if ((simple_rand() % 10) == 0) {
            int sparkle_x = simple_rand() % SCREEN_WIDTH;
            int sparkle_y = simple_rand() % SCREEN_HEIGHT;
            char sparkle_char = MATRIX_CHARS[simple_rand() % MATRIX_CHARS_LEN];
            screen_buffer[sparkle_y][sparkle_x] = sparkle_char;
        }
    }
    if ((simple_rand() % 30) == 0)
        for (int i = 0; i < 10; i++)
            screen_buffer[simple_rand() % SCREEN_HEIGHT][simple_rand() % SCREEN_WIDTH] = ' ';
}

void draw_matrix_scene(void) {
    for (int y = 0; y < SCREEN_HEIGHT; y++)
        for (int x = 0; x < SCREEN_WIDTH; x++)
            if ((simple_rand() % 100) == 0) screen_buffer[y][x] = ' ';

    for (int i = 0; i < MAX_DROPS; i++) {
        if (!drops[i].active) continue;
        int head_y = drops[i].y;
        if (head_y >= 0 && head_y < SCREEN_HEIGHT) {
            vga_putchar_color(MATRIX_CHARS[drops[i].head_char], VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        }
        for (int j = 1; j < drops[i].length; j++) {
            int trail_y = drops[i].y - j;
            if (trail_y >= 0 && trail_y < SCREEN_HEIGHT) {
                if ((simple_rand() % (j + 1)) == 0)
                    vga_putchar_color(drops[i].chars[j], VGA_COLOR_GREEN, VGA_COLOR_BLACK);
            }
        }
    }

    add_sparkles();

    vga_clear();
    vga_println_color("=== MATRIX DIGITAL RAIN ===", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_println_color("Press any key to return to LemonOS", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_println("");

    for (int y = 3; y < SCREEN_HEIGHT - 2; y++) {
        vga_print("  ");
        for (int x = 0; x < SCREEN_WIDTH - 4; x++) {
            char c = screen_buffer[y][x];
            if (c == ' ' && (simple_rand() % 50) == 0) {
                c = MATRIX_CHARS[simple_rand() % MATRIX_CHARS_LEN];
                screen_buffer[y][x] = c;
            }
            if (c != ' ') vga_putchar_color(c, VGA_COLOR_GREEN + (simple_rand() % 2), VGA_COLOR_BLACK);
            else vga_putchar(' ');
        }
        vga_putchar('\n');
    }

    vga_println("");
    vga_print("  ");
    for (int i = 0; i < 60; i++)
        vga_putchar_color(MATRIX_CHARS[(tick_counter + i * 7) % MATRIX_CHARS_LEN], VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_println("");
}

void matrix_screensaver(void) {
    screen_saver_active = 1;
    init_matrix_drops();
    while (screen_saver_active) {
        update_tick();
        update_matrix_drops();
        draw_matrix_scene();

        poll_keyboard();
        if (has_key()) {
            getchar();
            update_activity_time();
            screen_saver_active = 0;
            break;
        }

        for (volatile int i = 0; i < 500000; i++) asm volatile ("nop");
    }

    vga_clear();
    vga_println_color("Returning to LemonOS...", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_println("");
}

void check_screensaver(void) {
    update_tick();
    if (should_activate_screensaver()) matrix_screensaver();
}
