#include "pong.h"
#include "vga.h"
#include "keyboard.h"

#define PADDLE_HEIGHT 4
#define FIELD_WIDTH 24
#define FIELD_HEIGHT 22

static int left_paddle_y, right_paddle_y;
static int ball_x, ball_y;
static int ball_dx, ball_dy;
static int left_score = 0, right_score = 0;
static int game_over = 0;
static int frame_count = 0;
static int ball_move_counter = 0; // Counter for ball movement timing

void init_pong(void) {
    // Center both paddles vertically
    left_paddle_y = (FIELD_HEIGHT - PADDLE_HEIGHT) / 2;
    right_paddle_y = (FIELD_HEIGHT - PADDLE_HEIGHT) / 2;
    
    // Center the ball
    ball_x = FIELD_WIDTH / 2;
    ball_y = FIELD_HEIGHT / 2;
    
    // Random initial direction
    ball_dx = 1;
    ball_dy = (ball_y % 3) - 1;
    if (ball_dy == 0) ball_dy = 1;
    
    left_score = 0;
    right_score = 0;
    game_over = 0;
    frame_count = 0;
    ball_move_counter = 0;
}

void draw_pong(void) {
    vga_clear();

    // Draw border and field
    vga_println_color("##############", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (int y = 0; y < FIELD_HEIGHT; y++) {
        vga_putchar_color('#', VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        
        for (int x = 0; x < FIELD_WIDTH; x++) {
            // Draw left paddle
            if (x == 0 && y >= left_paddle_y && y < left_paddle_y + PADDLE_HEIGHT) {
                vga_putchar_color('|', VGA_COLOR_CYAN, VGA_COLOR_BLACK);
            }
            // Draw right paddle
            else if (x == FIELD_WIDTH - 1 && y >= right_paddle_y && y < right_paddle_y + PADDLE_HEIGHT) {
                vga_putchar_color('|', VGA_COLOR_MAGENTA, VGA_COLOR_BLACK);
            }
            // Draw ball
            else if (x == ball_x && y == ball_y) {
                vga_putchar_color('O', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            }
            // Draw center line
            else if (x == FIELD_WIDTH / 2) {
                vga_putchar_color('.', VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            }
            // Empty space
            else {
                vga_putchar_color(' ', VGA_COLOR_BLACK, VGA_COLOR_BLACK);
            }
        }
        
        vga_println_color("#", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }
    vga_println_color("##############", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    // Draw score
    vga_print_color("Score: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Left score
    if (left_score == 0) {
        vga_putchar_color('0', VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    } else {
        char temp[10]; int temp_pos = 0;
        int num = left_score;
        while (num > 0) {
            temp[temp_pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (temp_pos > 0) vga_putchar_color(temp[--temp_pos], VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    }
    
    vga_print_color(" - ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Right score
    if (right_score == 0) {
        vga_putchar_color('0', VGA_COLOR_MAGENTA, VGA_COLOR_BLACK);
    } else {
        char temp[10]; int temp_pos = 0;
        int num = right_score;
        while (num > 0) {
            temp[temp_pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (temp_pos > 0) vga_putchar_color(temp[--temp_pos], VGA_COLOR_MAGENTA, VGA_COLOR_BLACK);
    }

    vga_println("");
    vga_println_color("W/S = Left, Up/Down = Right, Q = Quit", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void update_pong(void) {
    frame_count++;
    ball_move_counter++;
    
    // Move ball only every 25 frames (about 2 pixels per second)
    // Assuming ~60fps: 60 frames/sec ÷ 25 = ~2.4 moves/sec ahemm i can do maths u monkeyyyy
    if (ball_move_counter < 25) {
        return;
    }
    
    ball_move_counter = 0; // Reset counter
    
    // Move ball
    ball_x += ball_dx;
    ball_y += ball_dy;
    
    // Top/bottom wall collision
    if (ball_y <= 0 || ball_y >= FIELD_HEIGHT - 1) {
        ball_dy = -ball_dy;
        ball_y = (ball_y <= 0) ? 0 : FIELD_HEIGHT - 1;
        
        // Add slight random angle change on wall bounce
        if (ball_y % 2 == 0) {
            ball_dy += (ball_y % 3) - 1;
            if (ball_dy > 1) ball_dy = 1;
            if (ball_dy < -1) ball_dy = -1;
        }
    }
    
    // Left paddle collision
    if (ball_x == 1 && ball_dx < 0) {
        if (ball_y >= left_paddle_y && ball_y < left_paddle_y + PADDLE_HEIGHT) {
            ball_dx = -ball_dx;
            
            // Add paddle spin holyy shiii
            int hit_pos = ball_y - left_paddle_y;
            if (hit_pos < PADDLE_HEIGHT / 3) {
                ball_dy = -1;
            } else if (hit_pos > 2 * PADDLE_HEIGHT / 3) {
                ball_dy = 1;
            }
        }
    }
    
    // Right paddle collision
    if (ball_x == FIELD_WIDTH - 2 && ball_dx > 0) {
        if (ball_y >= right_paddle_y && ball_y < right_paddle_y + PADDLE_HEIGHT) {
            ball_dx = -ball_dx;
            
            // Add paddle spin
            int hit_pos = ball_y - right_paddle_y;
            if (hit_pos < PADDLE_HEIGHT / 3) {
                ball_dy = -1;
            } else if (hit_pos > 2 * PADDLE_HEIGHT / 3) {
                ball_dy = 1;
            }
        }
    }
    
    // Score detection
    if (ball_x < 0) {
        right_score++;
        ball_x = FIELD_WIDTH / 2;
        ball_y = FIELD_HEIGHT / 2;
        ball_dx = 1;
        ball_dy = (ball_y % 3) - 1;
        if (ball_dy == 0) ball_dy = 1;
        frame_count = 0;
        ball_move_counter = 0;
    } else if (ball_x >= FIELD_WIDTH) {
        left_score++;
        ball_x = FIELD_WIDTH / 2;
        ball_y = FIELD_HEIGHT / 2;
        ball_dx = -1;
        ball_dy = (ball_y % 3) - 1;
        if (ball_dy == 0) ball_dy = 1;
        frame_count = 0;
        ball_move_counter = 0;
    }
    
    // Game over condition
    if (left_score >= 5 || right_score >= 5) {
        game_over = 1;
    }
}

static void handle_pong_input(void) {
    poll_keyboard();
    if (has_key()) {
        char key = getchar();
        switch (key) {
            case 'w': 
                if (left_paddle_y > 0) {
                    left_paddle_y--; 
                }
                break;
            case 's': 
                if (left_paddle_y < FIELD_HEIGHT - PADDLE_HEIGHT) {
                    left_paddle_y++; 
                }
                break;
            case 0x11: // Up arrow
                if (right_paddle_y > 0) {
                    right_paddle_y--; 
                }
                break;
            case 0x12: // Down arrow
                if (right_paddle_y < FIELD_HEIGHT - PADDLE_HEIGHT) {
                    right_paddle_y++; 
                }
                break;
            case 'q': 
                game_over = 1; 
                break;
        }
    }
}

void pong_game(void) {
    vga_println_color("Starting Pong Game...", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    vga_println_color("First to 5 points wins!", VGA_COLOR_CYAN, VGA_COLOR_BLACK);

    for (volatile int i = 0; i < 2000000; i++);

    init_pong();

    while (!game_over) {
        handle_pong_input();
        update_pong();
        draw_pong();
        for (volatile int i = 0; i < 50000; i++); // Reduced delay for smoother input still flickers af
    }

    vga_clear();
    vga_println_color("=== GAME OVER ===", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    
    if (left_score > right_score) {
        vga_println_color("Left Player Wins!", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    } else {
        vga_println_color("Right Player Wins!", VGA_COLOR_MAGENTA, VGA_COLOR_BLACK);
    }
    
    vga_print_color("Final Score: ", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    // Left score
    if (left_score == 0) {
        vga_putchar_color('0', VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    } else {
        char temp[10]; int temp_pos = 0;
        int num = left_score;
        while (num > 0) {
            temp[temp_pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (temp_pos > 0) vga_putchar_color(temp[--temp_pos], VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    }
    
    vga_print_color(" - ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Right score
    if (right_score == 0) {
        vga_putchar_color('0', VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    } else {
        char temp[10]; int temp_pos = 0;
        int num = right_score;
        while (num > 0) {
            temp[temp_pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (temp_pos > 0) vga_putchar_color(temp[--temp_pos], VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    }

    vga_println_color("", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_println_color("Press any key...", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    while (!has_key()) poll_keyboard();
    getchar();
}
