#include "screensaver.h"
#include "vga.h"
#include "keyboard.h"
#include <stdint.h>

#define MATRIX_CHARS " 01abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()_+-=[]{}|;:,.<>?/~"
#define MATRIX_CHARS_LEN 85
#define MAX_DROPS 100
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25

// Matrix drop structure for advanced effects
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

// Simple random number generator
unsigned int simple_rand(void) {
    static unsigned int seed = 12345;
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
    return seed;
}

// Update tick counter
void update_tick(void) {
    tick_counter++;
}

// Update activity time
void update_activity_time(void) {
    last_activity = tick_counter;
    screen_saver_active = 0;
}

// Check if screen saver should activate
int should_activate_screensaver(void) {
    unsigned long timeout = 3000000; // ~30 seconds for testing
    return ((tick_counter - last_activity) > timeout) && !screen_saver_active;
}

// Initialize matrix drops
void init_matrix_drops(void) {
    for (int i = 0; i < MAX_DROPS; i++) {
        drops[i].x = simple_rand() % SCREEN_WIDTH;
        drops[i].y = -(simple_rand() % 20); // Start above screen
        drops[i].speed = 1 + (simple_rand() % 3);
        drops[i].length = 5 + (simple_rand() % 20);
        drops[i].brightness = 255;
        drops[i].active = 1;
        drops[i].head_char = simple_rand() % MATRIX_CHARS_LEN;
        
        // Initialize drop characters with random values
        for (int j = 0; j < drops[i].length; j++) {
            drops[i].chars[j] = MATRIX_CHARS[simple_rand() % MATRIX_CHARS_LEN];
        }
    }
    
    // Clear screen buffer
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            screen_buffer[y][x] = ' ';
        }
    }
}

// Update matrix drops with advanced physics
void update_matrix_drops(void) {
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!drops[i].active) {
            // Occasionally reactivate dead drops
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
        
        // Move drop down
        drops[i].y += drops[i].speed;
        
        // Update head character occasionally
        if ((tick_counter % (5 + i)) == 0) {
            drops[i].head_char = simple_rand() % MATRIX_CHARS_LEN;
        }
        
        // Update trail characters occasionally
        if ((simple_rand() % 20) == 0) {
            int trail_pos = simple_rand() % drops[i].length;
            drops[i].chars[trail_pos] = MATRIX_CHARS[simple_rand() % MATRIX_CHARS_LEN];
        }
        
        // Fade out and reset when drop goes off screen
        if (drops[i].y > SCREEN_HEIGHT + drops[i].length) {
            drops[i].brightness -= 10;
            if (drops[i].brightness <= 0) {
                drops[i].active = 0;
            }
        }
    }
}

// Add random sparkle effects
void add_sparkles(void) {
    // Add random characters at random positions
    for (int i = 0; i < 5; i++) {
        if ((simple_rand() % 10) == 0) {
            int sparkle_x = simple_rand() % SCREEN_WIDTH;
            int sparkle_y = simple_rand() % SCREEN_HEIGHT;
            char sparkle_char = MATRIX_CHARS[simple_rand() % MATRIX_CHARS_LEN];
            screen_buffer[sparkle_y][sparkle_x] = sparkle_char;
        }
    }
    
    // Occasionally clear random spots
    if ((simple_rand() % 30) == 0) {
        for (int i = 0; i < 10; i++) {
            int clear_x = simple_rand() % SCREEN_WIDTH;
            int clear_y = simple_rand() % SCREEN_HEIGHT;
            screen_buffer[clear_y][clear_x] = ' ';
        }
    }
}

// Draw the entire matrix scene
void draw_matrix_scene(void) {
    // Clear screen buffer but keep some persistence
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            // Fade out existing characters occasionally
            if ((simple_rand() % 100) == 0) {
                screen_buffer[y][x] = ' ';
            }
        }
    }
    
    // Draw active drops
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!drops[i].active) continue;
        
        // Draw drop head (brightest)
        int head_y = drops[i].y;
        if (head_y >= 0 && head_y < SCREEN_HEIGHT) {
            screen_buffer[head_y][drops[i].x] = MATRIX_CHARS[drops[i].head_char];
        }
        
        // Draw drop trail
        for (int j = 1; j < drops[i].length; j++) {
            int trail_y = drops[i].y - j;
            if (trail_y >= 0 && trail_y < SCREEN_HEIGHT) {
                // Fade trail based on position
                if ((simple_rand() % (j + 1)) == 0) { // More fading for longer trails
                    screen_buffer[trail_y][drops[i].x] = drops[i].chars[j];
                }
            }
        }
    }
    
    // Add sparkle effects
    add_sparkles();
    
    // Render to screen
    vga_clear();
    
    // Draw title
    vga_println("=== MATRIX DIGITAL RAIN ===");
    vga_println("Press any key to return to LemonOS");
    vga_println("");
    
    // Draw the matrix scene
    for (int y = 3; y < SCREEN_HEIGHT - 2; y++) {
        vga_print("  "); // Left margin
        
        for (int x = 0; x < SCREEN_WIDTH - 4; x++) {
            char c = screen_buffer[y][x];
            
            // Occasionally add random new characters
            if (c == ' ' && (simple_rand() % 50) == 0) {
                c = MATRIX_CHARS[simple_rand() % MATRIX_CHARS_LEN];
                screen_buffer[y][x] = c;
            }
            
            vga_putchar(c);
        }
        vga_putchar('\n');
    }
    
    // Draw footer with cool effects
    vga_println("");
    vga_print("  ");
    for (int i = 0; i < 60; i++) {
        char footer_char = MATRIX_CHARS[(tick_counter + i * 7) % MATRIX_CHARS_LEN];
        vga_putchar(footer_char);
    }
    vga_println("");
}

// Ultra cool matrix screen saver
void matrix_screensaver(void) {
    screen_saver_active = 1;
    
    // Initialize drops
    init_matrix_drops();
    
    int frame_count = 0;
    
    while (screen_saver_active) {
        update_tick();
        
        // Update physics
        update_matrix_drops();
        
        // Draw everything
        draw_matrix_scene();
        
        frame_count++;
        
        // exit on key press
        poll_keyboard();
        if (has_key()) {
            getchar();
            update_activity_time();
            screen_saver_active = 0;
            break;
        }
        
        // speed control
        for (volatile int i = 0; i < 500000; i++) {
            asm volatile ("nop");
        }
    }
    
    vga_clear();
    vga_println("Returning to LemonOS...");
    vga_println("");
}

// Public interface
void check_screensaver(void) {
    update_tick();
    
    if (should_activate_screensaver()) {
        matrix_screensaver();
    }
}
