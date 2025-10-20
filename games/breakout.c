#include "breakout.h"
#include "vga.h"
#include "keyboard.h"
#include "vga_utils.h"
#include <stdint.h>

#define GAME_WIDTH 80
#define GAME_HEIGHT 24
#define PADDLE_WIDTH 10
#define BALL_SPEED 1
#define BRICK_WIDTH 6
#define BRICK_HEIGHT 1


static int paddle_x;
static int ball_x, ball_y;
static int ball_dx, ball_dy;
static int bricks[ROWS][COLS];
static int score = 0;
static int lives = LIVES;
static int bricks_remaining = ROWS * COLS;
static int game_running = 1;

// Colors for bricks
static uint8_t brick_colors[] = {
    VGA_COLOR_BLUE,
    VGA_COLOR_GREEN, 
    VGA_COLOR_YELLOW,
    VGA_COLOR_RED,
    VGA_COLOR_MAGENTA
};

static void delay(int milliseconds) {
    for (volatile int i = 0; i < milliseconds * 1000; i++) {
        asm volatile ("nop");
    }
}

// Initialize game
static void init_game(void) {
    paddle_x = (GAME_WIDTH - PADDLE_WIDTH) / 2;
    ball_x = GAME_WIDTH / 2;
    ball_y = GAME_HEIGHT - 5;
    ball_dx = BALL_SPEED;
    ball_dy = -BALL_SPEED;
    
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            bricks[row][col] = 1;
        }
    }
    
    score = 0;
    lives = LIVES;
    bricks_remaining = ROWS * COLS;
    game_running = 1;
}


static void draw_game(void) {
    vga_clear();
    
    // Draw top border
    for (int x = 0; x < GAME_WIDTH; x++) {
        vga_putchar_color('=', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
    vga_println("");
    
    // Draw play area
    for (int y = 1; y < GAME_HEIGHT - 1; y++) {
        vga_putchar_color('|', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        for (int x = 1; x < GAME_WIDTH - 1; x++) {
            int drawn = 0;
            
            // Draw ball
            if (x == ball_x && y == ball_y) {
                vga_putchar_color('O', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                drawn = 1;
            }
            
            // Draw paddle
            if (!drawn && y == GAME_HEIGHT - 2 && x >= paddle_x && x < paddle_x + PADDLE_WIDTH) {
                vga_putchar_color('=', VGA_COLOR_CYAN, VGA_COLOR_BLACK);
                drawn = 1;
            }
            
            // Draw bricks
            if (!drawn) {
                for (int row = 0; row < ROWS && !drawn; row++) {
                    for (int col = 0; col < COLS && !drawn; col++) {
                        if (bricks[row][col]) {
                            int brick_x_start = col * (BRICK_WIDTH + 1) + 2;
                            int brick_y = row * (BRICK_HEIGHT + 1) + 2;
                            if (y == brick_y && x >= brick_x_start && x < brick_x_start + BRICK_WIDTH) {
                                vga_putchar_color('#', brick_colors[row % 5], VGA_COLOR_BLACK);
                                drawn = 1;
                            }
                        }
                    }
                }
            }
            
            // Empty space
            if (!drawn) {
                vga_putchar(' ');
            }
        }
        
        vga_putchar_color('|', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_println("");
    }
    
    // Draw bottom border
    for (int x = 0; x < GAME_WIDTH; x++) {
        vga_putchar_color('=', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
    vga_println("");
    
    // Draw ui
    vga_print_color("Score: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Print score manually
    if (score == 0) {
        vga_putchar_color('0', VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    } else {
        char temp[16];
        int pos = 0;
        int num = score;
        while (num > 0) {
            temp[pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (pos > 0) {
            vga_putchar_color(temp[--pos], VGA_COLOR_GREEN, VGA_COLOR_BLACK);
        }
    }
    
    vga_print_color(" | Lives: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (int i = 0; i < lives; i++) {
        vga_putchar_color(0x03, VGA_COLOR_RED, VGA_COLOR_BLACK);
    }
    
    vga_print_color(" | Bricks: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Print bricks remaining manually
    if (bricks_remaining == 0) {
        vga_putchar_color('0', VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    } else {
        char temp[16];
        int pos = 0;
        int num = bricks_remaining;
        while (num > 0) {
            temp[pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (pos > 0) {
            vga_putchar_color(temp[--pos], VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        }
    }
    
    vga_println("");
    vga_println_color("Controls: A/Left = Move Left, D/Right = Move Right, Q = Quit", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}


static void handle_input(void) {
    poll_keyboard();
    
    if (has_key()) {
        char c = getchar();
        
        if (c == 'a' || c == 'A') {
            if (paddle_x > 1) {
                paddle_x -= 2;
            }
        } else if (c == 'd' || c == 'D') {
            if (paddle_x + PADDLE_WIDTH < GAME_WIDTH - 1) {
                paddle_x += 2;
            }
        } else if (c == 'q' || c == 'Q') {
            game_running = 0;
        }
    }
}


static void update_game(void) {
    // Move ball
    ball_x += ball_dx;
    ball_y += ball_dy;
    
    // Wall collisions
    if (ball_x <= 1 || ball_x >= GAME_WIDTH - 2) {
        ball_dx = -ball_dx;
        ball_x = (ball_x <= 1) ? 1 : GAME_WIDTH - 2;
    }
    if (ball_y <= 1) {
        ball_dy = -ball_dy;
        ball_y = 1;
    }
    
    // Paddle collision
    if (ball_y == GAME_HEIGHT - 3 && ball_x >= paddle_x && ball_x < paddle_x + PADDLE_WIDTH) {
        ball_dy = -ball_dy;
        ball_y = GAME_HEIGHT - 3;
    }
    
    // Brick collisions
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            if (bricks[row][col]) {
                int brick_x_start = col * (BRICK_WIDTH + 1) + 2;
                int brick_y = row * (BRICK_HEIGHT + 1) + 2;
                
                if (ball_x >= brick_x_start && ball_x < brick_x_start + BRICK_WIDTH &&
                    ball_y == brick_y) {
                    bricks[row][col] = 0;
                    bricks_remaining--;
                    score += 10;
                    ball_dy = -ball_dy;
                    return;
                }
            }
        }
    }
    
    // Bottom collision - lose life
    if (ball_y >= GAME_HEIGHT - 1) {
        lives--;
        if (lives > 0) {
            ball_x = GAME_WIDTH / 2;
            ball_y = GAME_HEIGHT - 5;
            ball_dx = BALL_SPEED;
            ball_dy = -BALL_SPEED;
            paddle_x = (GAME_WIDTH - PADDLE_WIDTH) / 2;
        } else {
            game_running = 0;
        }
    }
}

// Main game function (this one stays public)
void breakout_game(void) {
    vga_clear();
    vga_println_color("=== BREAKOUT GAME ===", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_println_color("Press any key to start...", VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    
    // Wait for key press
    while (!has_key()) {
        poll_keyboard();
        delay(10);
    }
    getchar();
    
    init_game();
    
    while (game_running && lives > 0 && bricks_remaining > 0) {
        handle_input();
        update_game();
        draw_game();
        
        delay(100);
    }
    
    // Game over screen
    vga_clear();
    if (bricks_remaining == 0) {
        vga_println_color("=== YOU WIN! ===", VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    } else {
        vga_println_color("=== GAME OVER ===", VGA_COLOR_RED, VGA_COLOR_BLACK);
    }
    
    vga_print_color("Final Score: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Print final score manually
    if (score == 0) {
        vga_putchar_color('0', VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    } else {
        char temp[16];
        int pos = 0;
        int num = score;
        while (num > 0) {
            temp[pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (pos > 0) {
            vga_putchar_color(temp[--pos], VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        }
    }
    
    vga_println("");
    vga_println_color("Press any key to continue...", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    while (!has_key()) {
        poll_keyboard();
        delay(10);
    }
    getchar();
    
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}
