#include "shutdown.h"
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

void shutdown(void) {
    vga_clear();
    vga_println("=== SYSTEM SHUTDOWN ===");
    vga_println("Goodbye from LemonOS! 🍋");
    vga_println("Shutting down in 5 seconds...");
    
    // Wait 1 second between each countdown
    vga_print("5...");
    wait_seconds(1);
    
    vga_print(" 4...");
    wait_seconds(1);
    
    vga_print(" 3...");
    wait_seconds(1);
    
    vga_print(" 2...");
    wait_seconds(1);
    
    vga_println(" 1...");
    wait_seconds(1);
    
    vga_println("Shutting down NOW!");
    
    // Small final delay to see the message
    for (volatile int i = 0; i < 1000000; i++);
    
    // Try different shutdown methods
    
    // Method 1: QEMU/Bochs shutdown (port 0x604)
    asm volatile ("outw %0, %1" : : "a"((short)0x2000), "Nd"((short)0x604));
    
    // Method 2: QEMU shutdown (port 0xB004)
    asm volatile ("outw %0, %1" : : "a"((short)0x2000), "Nd"((short)0xB004));
    
    // Method 3: VirtualBox shutdown (port 0x4004)
    asm volatile ("outw %0, %1" : : "a"((short)0x3400), "Nd"((short)0x4004));
    
    // If still running, try CPU halt
    vga_println("Shutdown failed. Halting CPU...");
    asm volatile ("hlt");
    
    // Infinite loop if halt doesn't work
    while (1) {
        asm volatile ("hlt");
    }
}
