#include "commands.h"
#include "vga.h"
#include "art.h"
#include "keyboard.h"
#include "calculator.h"
#include "clock.h"
#include "system_info.h"
#include "vga_utils.h"
#include "unit_converter.h"
#include "screensaver.h" 
#include "file_manager.h" 
#include "text_graphics.h"
#include "snake.h"
#include "shutdown.h"
#include "reboot.h" 
#include "gui.h"
#include "color_demo.h"
#include "breakout.h"
#include "pong.h"

// Simple string length function
int kstrlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

// FIXED: Make strcmp static to avoid conflict with drivers/string.c
static int strcmp(const char* a, const char* b) {
   while (*a && (*a == *b)) {
       a++;
       b++;
   }
   return *(const unsigned char*)a - *(const unsigned char*)b;
}

void run_command(const char* cmd) {
    // Update activity time on any command
    update_activity_time();
    
    if (cmd[0] == '\0') {
        return;
    }

    if (strcmp(cmd, "help") == 0) {
        vga_println("Commands: help, clear, about, calc, clock, info");
        vga_println("See the guide for all commands....");
        vga_println("  help  - Show this help message");
        vga_println("  clear - Clear the screen");
        vga_println("  about - Show system information");
        vga_println("  calc  - Simple calculator");
        vga_println("  clock - Real-time clock with RTC");
    } 
    else if (strcmp(cmd, "clear") == 0) {
        vga_clear();
    } 
    else if (strcmp(cmd, "about") == 0) {
        for (int i = 0; logo[i] != 0; i++) {
            vga_println(logo[i]);
        }
        vga_println("LemonOS v3.0 - Baremetal OS by Dwip");
    }
    else if (strcmp(cmd, "convert") == 0) {
        unit_converter_command();
    }
    else if (strcmp(cmd, "matrix") == 0) {
        matrix_screensaver();
    }
    else if (strcmp(cmd, "snake") == 0) {
           snake_game();
    }
    else if (strcmp(cmd, "files") == 0) {
        file_manager_command();
    }
    else if (strcmp(cmd, "calc") == 0) {
        calc_command();
    }
    else if (strcmp(cmd, "clock") == 0) {
        clock_command();
    }
    else if (strcmp(cmd, "info") == 0) {
        system_info_command();
    }
    else if (strcmp(cmd, "shutdown") == 0) {
        shutdown();
    }
    else if (strcmp(cmd, "reboot") == 0) {
        reboot();
    }
    else if (strcmp(cmd, "gui") == 0) {
        gui_demo();
    }
    else if (strcmp(cmd, "colors") == 0) {
        color_demo();
    }
    else if (strcmp(cmd, "breakout") == 0) {
        breakout_game();
    }
    else if (strcmp(cmd, "pong") == 0) {
        pong_game();
    }
    else {
        vga_print("Unknown command: ");
        vga_println(cmd);
        vga_println("Type 'help' for available commands");
    }
}
