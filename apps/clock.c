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

// Display clock with RTC data
void display_clock(void) {
    int hours, minutes, seconds, day, month, year;
    
    get_rtc_time(&hours, &minutes, &seconds);
    get_rtc_date(&day, &month, &year);
    
    vga_print("\r");
    
    vga_print("Date: ");
    vga_print_int(day);
    vga_putchar('/');
    vga_print_int(month);
    vga_putchar('/');
    vga_print_int(year);
    vga_print(" | Time: ");
    
    if (hours < 10) vga_putchar('0');
    vga_print_int(hours);
    vga_putchar(':');
    if (minutes < 10) vga_putchar('0');
    vga_print_int(minutes);
    vga_putchar(':');
    if (seconds < 10) vga_putchar('0');
    vga_print_int(seconds);
    
    vga_print(" (");
    if (hours == 0) {
        vga_print_int(12);
        vga_print(" AM");
    } else if (hours < 12) {
        vga_print_int(hours);
        vga_print(" AM");
    } else if (hours == 12) {
        vga_print_int(12);
        vga_print(" PM");
    } else {
        vga_print_int(hours - 12);
        vga_print(" PM");
    }
    vga_print(")");
    
    vga_print("                ");
}

// Clock command implementation
void clock_command(void) {
    vga_println("=== LemonOS Real-Time Clock ===");
    vga_println("Displaying real time from RTC hardware");
    vga_println("Press any key to exit");
    vga_println("");
    
    display_clock();
    
    while(1) {
        poll_keyboard();
        if (has_key()) {
            getchar(); // Read and discard the key
            break;
        }
        
        delay(1000);
        display_clock();
    }
    
    vga_println("");
    vga_println("=== Clock Closed ===");
}
