#include "reboot.h"
#include "vga.h"
#include <stdint.h>
#include "rtc.h"

// Get current RTC seconds
static int get_rtc_seconds(void) {
    int hours, minutes, seconds;
    get_rtc_time(&hours, &minutes, &seconds);
    return seconds;
}

// Wait for specified seconds using RTC
static void wait_seconds(int seconds) {
    int start_second = get_rtc_seconds();
    int target_second = (start_second + seconds) % 60;
    
    // Wait for the target second
    while (get_rtc_seconds() != target_second) {
        // Small delay to avoid busy looping too hard
        for (volatile int i = 0; i < 1000; i++);
    }
}

void reboot(void) {
    vga_clear();
    vga_println("=== SYSTEM REBOOT ===");
    vga_println("Restarting LemonOS in 3 seconds...");
    
    // Wait 1 second between each countdown
    vga_print("3...");
    wait_seconds(1);
    
    vga_print(" 2...");
    wait_seconds(1);
    
    vga_println(" 1...");
    wait_seconds(1);
    
    vga_println("Rebooting NOW!");
    
    // Small final delay to see the message
    for (volatile int i = 0; i < 1000000; i++);
    
    // Trigger keyboard controller reset
    asm volatile ("cli");
    
    // Wait for keyboard controller to be ready
    unsigned char good;
    do {
        asm volatile ("inb $0x64, %0" : "=a"(good));
    } while (good & 0x02);
    
    // Send reset command
    asm volatile ("outb %0, $0x64" : : "a"((unsigned char)0xFE));
    
    // If that fails, triple fault
    asm volatile ("ud2");
    
    // Infinite halt as last resort
    while (1) {
        asm volatile ("hlt");
    }
}
