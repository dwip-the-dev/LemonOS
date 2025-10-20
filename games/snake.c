#include "snake.h"
#include "vga.h"
#include "keyboard.h"

static int snake_x[100], snake_y[100];
static int snake_length = 3;
static int food_x, food_y;
static int dir = 1; // 0=up,1=right,2=down,3=left
static int game_over = 0;
static int score = 0;
static int moved_this_frame = 0;

void init_snake(void) {
    snake_length = 3;
    snake_x[0] = 5; snake_y[0] = 5;
    snake_x[1] = 4; snake_y[1] = 5;
    snake_x[2] = 3; snake_y[2] = 5;

    food_x = 8;
    food_y = 8;

    game_over = 0;
    score = 0;
    dir = 1;
    moved_this_frame = 0;
}

void draw_game(void) {
    vga_clear();

    // Draw border
    vga_println_color("############", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (int y = 0; y < 10; y++) {
        vga_putchar_color('#', VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        for (int x = 0; x < 10; x++) {
            int is_snake = 0;
            for (int i = 0; i < snake_length; i++) {
                if (x == snake_x[i] && y == snake_y[i]) {
                    if (i == 0)
                        vga_putchar_color('O', VGA_COLOR_GREEN, VGA_COLOR_BLACK); // head
                    else
                        vga_putchar_color('o', VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK); // body
                    is_snake = 1;
                    break;
                }
            }
            if (!is_snake) {
                if (x == food_x && y == food_y)
                    vga_putchar_color('*', VGA_COLOR_RED, VGA_COLOR_BLACK);
                else
                    vga_putchar_color(' ', VGA_COLOR_BLACK, VGA_COLOR_BLACK);
            }
        }
        vga_println_color("#", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }
    vga_println_color("############", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    // Draw info
    vga_print_color("Score: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    char score_str[10];
    int len = 0, score_num = score;
    if (score_num == 0) score_str[len++] = '0';
    else {
        char temp[10]; int temp_pos = 0;
        while (score_num > 0) {
            temp[temp_pos++] = '0' + (score_num % 10);
            score_num /= 10;
        }
        while (temp_pos > 0) score_str[len++] = temp[--temp_pos];
    }
    score_str[len] = '\0';
    vga_println_color(score_str, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);

    vga_println_color("WASD to move 1 step, Q to quit", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void update_game(void) {
    // Only move if we pressed a key this frame
    if (!moved_this_frame) {
        return;
    }
    
    // Reset movement flag
    moved_this_frame = 0;
    
    // Move snake body (from tail to head)
    for (int i = snake_length; i > 0; i--) {
        snake_x[i] = snake_x[i-1];
        snake_y[i] = snake_y[i-1];
    }
    
    // Move head based on direction (1 pixel/step)
    switch (dir) {
        case 0: snake_y[0]--; break; // up - 1 step
        case 1: snake_x[0]++; break; // right - 1 step
        case 2: snake_y[0]++; break; // down - 1 step
        case 3: snake_x[0]--; break; // left - 1 step
    }
    
    // Check wall collision (play area: 0-9 x 0-9)
    if (snake_x[0] < 0 || snake_x[0] >= 10 || snake_y[0] < 0 || snake_y[0] >= 10) {
        game_over = 1;
        return;
    }
    
    // Check self collision (skip head)
    for (int i = 1; i < snake_length; i++) {
        if (snake_x[0] == snake_x[i] && snake_y[0] == snake_y[i]) {
            game_over = 1;
            return;
        }
    }
    
    // Check food collision
    if (snake_x[0] == food_x && snake_y[0] == food_y) {
        snake_length++;
        score += 10;
        
        // New food position
        food_x = (food_x + 5) % 8 + 1;
        food_y = (food_y + 3) % 8 + 1;
        
        // Make sure food doesn't spawn on snake
        for (int i = 0; i < snake_length; i++) {
            if (food_x == snake_x[i] && food_y == snake_y[i]) {
                food_x = (food_x + 2) % 8 + 1;
                food_y = (food_y + 1) % 8 + 1;
                i = -1; // restart check
            }
        }
    }
}

// FIXED: Changed to static function
static void handle_input(void) {
    poll_keyboard();
    if (has_key()) {
        char key = getchar();
        switch (key) {
            case 'w': 
                if (dir != 2) {
                    dir = 0; 
                    moved_this_frame = 1;
                }
                break;
            case 'd': 
                if (dir != 3) {
                    dir = 1; 
                    moved_this_frame = 1;
                }
                break;
            case 's': 
                if (dir != 0) {
                    dir = 2; 
                    moved_this_frame = 1;
                }
                break;
            case 'a': 
                if (dir != 1) {
                    dir = 3; 
                    moved_this_frame = 1;
                }
                break;
            case 'q': 
                game_over = 1; 
                break;
        }
    }
}

void snake_game(void) {
    vga_println_color("Starting Snake Game...", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    vga_println_color("Press WASD to move 1 step at a time", VGA_COLOR_CYAN, VGA_COLOR_BLACK);

    for (volatile int i = 0; i < 2000000; i++);

    init_snake();

    while (!game_over) {
        handle_input();
        update_game();
        draw_game();
        for (volatile int i = 0; i < 100000; i++);
    }

    vga_clear();
    vga_println_color("=== GAME OVER ===", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_print_color("Final Score: ", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    int score_num = score;
    if (score_num == 0) vga_putchar_color('0', VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    else {
        char temp[10]; int temp_pos = 0;
        while (score_num > 0) {
            temp[temp_pos++] = '0' + (score_num % 10);
            score_num /= 10;
        }
        while (temp_pos > 0)
            vga_putchar_color(temp[--temp_pos], VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    }

    vga_println_color("", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_println_color("Press any key...", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    while (!has_key()) poll_keyboard();
    getchar();
}
