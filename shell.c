#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "commands.h"
#include "art.h"
#include "screensaver.h"

#define INPUT_BUFFER 128

// Print prompt
static void shell_prompt(void) {
    vga_print("LemonOS> ");
}

void shell_run(void) {
    char input[INPUT_BUFFER];
    int pos = 0;

    vga_clear();

    // Print ASCII logo
    for (int i = 0; logo[i] != 0; i++) {
        for (int j = 0; logo[i][j] != '\0'; j++) {
            vga_putchar(logo[i][j]);
        }
        vga_putchar('\n');
    }

    vga_println("Welcome to LemonOS! Type 'help' for commands.");
    
    // Initialize activity time
    update_activity_time();
    
    shell_prompt();

    while (1) {
        // Update tick counter
        update_tick();
        
        // Check for screen saver activation
        check_screensaver();
        
        poll_keyboard();
        if (has_key()) {
            char c = getchar();
            
            // Update activity time on ANY key press
            update_activity_time();

            if (c == '\n') {
                input[pos] = '\0';
                vga_putchar('\n');
                run_command(input);
                shell_prompt();
                pos = 0;
            }
            else if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    vga_putchar('\b');
                }
            }
            else {
                if (pos < INPUT_BUFFER - 1) {
                    input[pos++] = c;
                    vga_putchar(c);
                }
            }
        }
        
        // Small delay to prevent excessive CPU usage
        for (volatile int i = 0; i < 10000; i++) {
            asm volatile ("nop");
        }
    }
}
