#include "clock.h"
#include "vga.h"
#include "keyboard.h"
#include "rtc.h"
#include "vga_utils.h"

// Simple delay function
void delay(int milliseconds) {
    for (volatile int i = 0; i < milliseconds * 10000; i++) {
        asm volatile ("nop");
    }
}

// Display clock with RTC data (single line)
void display_clock(void) {
    int hours, minutes, seconds, day, month, year;
    
    get_rtc_time(&hours, &minutes, &seconds);
    get_rtc_date(&day, &month, &year);
    
    // Move cursor to beginning of the clock line (line 3)
    vga_set_cursor(0, 3);
    
    // Clear the entire line first
    vga_print("                                                                                ");
    vga_set_cursor(0, 3);
    
    // Date in green
    vga_print_color("Date: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    if (day < 10) vga_putchar_color('0', VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    vga_print_color("", VGA_COLOR_GREEN, VGA_COLOR_BLACK); // Set color for number
    // Print day
    if (day == 0) {
        vga_putchar_color('0', VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    } else {
        char temp[3];
        int temp_pos = 0;
        int num = day;
        
        while (num > 0) {
            temp[temp_pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (temp_pos > 0) {
            vga_putchar_color(temp[--temp_pos], VGA_COLOR_GREEN, VGA_COLOR_BLACK);
        }
    }
    
    vga_putchar_color('/', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    if (month < 10) vga_putchar_color('0', VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    // Print month
    if (month == 0) {
        vga_putchar_color('0', VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    } else {
        char temp[3];
        int temp_pos = 0;
        int num = month;
        
        while (num > 0) {
            temp[temp_pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (temp_pos > 0) {
            vga_putchar_color(temp[--temp_pos], VGA_COLOR_GREEN, VGA_COLOR_BLACK);
        }
    }
    
    vga_putchar_color('/', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Print year
    if (year == 0) {
        vga_putchar_color('0', VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    } else {
        char temp[5];
        int temp_pos = 0;
        int num = year;
        
        while (num > 0) {
            temp[temp_pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (temp_pos > 0) {
            vga_putchar_color(temp[--temp_pos], VGA_COLOR_GREEN, VGA_COLOR_BLACK);
        }
    }
    
    vga_print_color(" | ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Time in cyan
    vga_print_color("Time: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Hours
    if (hours < 10) vga_putchar_color('0', VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    if (hours == 0) {
        vga_putchar_color('0', VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    } else {
        char temp[3];
        int temp_pos = 0;
        int num = hours;
        
        while (num > 0) {
            temp[temp_pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (temp_pos > 0) {
            vga_putchar_color(temp[--temp_pos], VGA_COLOR_CYAN, VGA_COLOR_BLACK);
        }
    }
    
    vga_putchar_color(':', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Minutes
    if (minutes < 10) vga_putchar_color('0', VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    if (minutes == 0) {
        vga_putchar_color('0', VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    } else {
        char temp[3];
        int temp_pos = 0;
        int num = minutes;
        
        while (num > 0) {
            temp[temp_pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (temp_pos > 0) {
            vga_putchar_color(temp[--temp_pos], VGA_COLOR_CYAN, VGA_COLOR_BLACK);
        }
    }
    
    vga_putchar_color(':', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Seconds
    if (seconds < 10) vga_putchar_color('0', VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    if (seconds == 0) {
        vga_putchar_color('0', VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    } else {
        char temp[3];
        int temp_pos = 0;
        int num = seconds;
        
        while (num > 0) {
            temp[temp_pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (temp_pos > 0) {
            vga_putchar_color(temp[--temp_pos], VGA_COLOR_CYAN, VGA_COLOR_BLACK);
        }
    }
    
    vga_print_color(" (", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // AM/PM in yellow
    if (hours == 0) {
        vga_print_color("12", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_print_color(" AM", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    } else if (hours < 12) {
        // Print hours for AM
        if (hours == 0) {
            vga_putchar_color('0', VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        } else {
            char temp[3];
            int temp_pos = 0;
            int num = hours;
            
            while (num > 0) {
                temp[temp_pos++] = '0' + (num % 10);
                num /= 10;
            }
            while (temp_pos > 0) {
                vga_putchar_color(temp[--temp_pos], VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
            }
        }
        vga_print_color(" AM", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    } else if (hours == 12) {
        vga_print_color("12", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_print_color(" PM", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    } else {
        // Print hours for PM (hours - 12)
        int pm_hours = hours - 12;
        if (pm_hours == 0) {
            vga_putchar_color('0', VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        } else {
            char temp[3];
            int temp_pos = 0;
            int num = pm_hours;
            
            while (num > 0) {
                temp[temp_pos++] = '0' + (num % 10);
                num /= 10;
            }
            while (temp_pos > 0) {
                vga_putchar_color(temp[--temp_pos], VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
            }
        }
        vga_print_color(" PM", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    }
    
    vga_print_color(")", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

// Clock command implementation
void clock_command(void) {
    vga_clear();
    vga_println_color("=== LemonOS Real-Time Clock ===", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_println_color("Displaying real time from RTC hardware", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_println_color("Press any key to exit", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_println(""); // Empty line before clock display
    
    // Initial display
    display_clock();
    
    while(1) {
        poll_keyboard();
        if (has_key()) {
            getchar(); // Read and discard the key
            break;
        }
        
        delay(1000); // Update every second
        display_clock();
    }
    
    // Move cursor to line after the clock display
    vga_set_cursor(0, 5);
    vga_println_color("=== Clock Closed ===", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    
    // Reset to default colors
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}
