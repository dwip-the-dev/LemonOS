#include "system_info.h"
#include "vga.h"
#include "keyboard.h"
#include "rtc.h"
#include "vga_utils.h"

// BIOS Data Area locations
#define BDA_BASE 0x400
#define BDA_EQUIPMENT_WORD (BDA_BASE + 0x10)
#define BDA_MEMORY_KB (BDA_BASE + 0x13)
#define BDA_DISPLAY_MODE (BDA_BASE + 0x49)

// CPUID function
void cpuid(uint32_t function, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    asm volatile(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(function)
    );
}

// Check if CPUID is supported
int has_cpuid(void) {
    uint32_t eflags;
    asm volatile(
        "pushfl\n"
        "popl %0\n"
        "movl %0, %%ecx\n"
        "xorl $0x200000, %0\n"
        "pushl %0\n"
        "popfl\n"
        "pushfl\n"
        "popl %0\n"
        "pushl %%ecx\n"
        "popfl\n"
        : "=r"(eflags)
        :
        : "ecx"
    );
    return (eflags & 0x200000) != 0;
}

// Get CPU vendor string
void get_cpu_vendor(char* vendor) {
    if (!has_cpuid()) {
        vendor[0] = 'N';
        vendor[1] = '/';
        vendor[2] = 'A';
        vendor[3] = '\0';
        return;
    }
    
    uint32_t eax, ebx, ecx, edx;
    cpuid(0, &eax, &ebx, &ecx, &edx);
    
    *(uint32_t*)(vendor) = ebx;
    *(uint32_t*)(vendor + 4) = edx;
    *(uint32_t*)(vendor + 8) = ecx;
    vendor[12] = '\0';
}

// Get CPU brand string
void get_cpu_brand(char* brand) {
    if (!has_cpuid()) {
        brand[0] = 'U';
        brand[1] = 'n';
        brand[2] = 'k';
        brand[3] = 'n';
        brand[4] = 'o';
        brand[5] = 'w';
        brand[6] = 'n';
        brand[7] = '\0';
        return;
    }
    
    uint32_t eax, ebx, ecx, edx;
    char* ptr = brand;
    
    // Get brand string (requires multiple CPUID calls)
    for (uint32_t i = 0x80000002; i <= 0x80000004; i++) {
        cpuid(i, &eax, &ebx, &ecx, &edx);
        *(uint32_t*)(ptr) = eax;
        *(uint32_t*)(ptr + 4) = ebx;
        *(uint32_t*)(ptr + 8) = ecx;
        *(uint32_t*)(ptr + 12) = edx;
        ptr += 16;
    }
    brand[48] = '\0';
    
    // Trim trailing spaces
    for (int i = 47; i >= 0; i--) {
        if (brand[i] == ' ') {
            brand[i] = '\0';
        } else {
            break;
        }
    }
}

// Get memory size from BIOS Data Area (conventional memory)
unsigned int get_conventional_memory(void) {
    uint16_t* mem_kb = (uint16_t*)BDA_MEMORY_KB;
    return *mem_kb; // Returns KB of conventional memory (0-640KB)
}

// Simple memory detection using BIOS INT 15h E820 (we'll simulate for now)
unsigned int get_extended_memory(void) {
    // In a real implementation, you'd use INT 15h, E820
    // For now, we'll return a placeholder
    return 64 * 1024; // 64MB placeholder
}

// Get equipment word from BIOS
uint16_t get_equipment_word(void) {
    uint16_t* equipment = (uint16_t*)BDA_EQUIPMENT_WORD;
    return *equipment;
}

// Parse equipment word
void parse_equipment(uint16_t equipment, char* display_type, int* floppy_drives, int* serial_ports) {
    // Display type
    uint8_t display_bits = (equipment >> 4) & 0x03;
    switch (display_bits) {
        case 0: display_type[0] = 'N'; display_type[1] = '/'; display_type[2] = 'A'; display_type[3] = '\0'; break;
        case 1: display_type[0] = 'C'; display_type[1] = 'G'; display_type[2] = 'A'; display_type[3] = '\0'; break;
        case 2: display_type[0] = 'M'; display_type[1] = 'D'; display_type[2] = 'A'; display_type[3] = '\0'; break;
        case 3: display_type[0] = 'E'; display_type[1] = 'G'; display_type[2] = 'A'; display_type[3] = '/'; display_type[4] = 'V'; display_type[5] = 'G'; display_type[6] = 'A'; display_type[7] = '\0'; break;
    }
    
    // Floppy drives
    *floppy_drives = ((equipment >> 6) & 0x03) + 1;
    if (*floppy_drives > 2) *floppy_drives = 0;
    
    // Serial ports
    *serial_ports = (equipment >> 9) & 0x07;
}

// Get VGA display mode
uint8_t get_display_mode(void) {
    uint8_t* mode = (uint8_t*)BDA_DISPLAY_MODE;
    return *mode;
}

// System info command
void system_info_command(void) {
    char cpu_vendor[13];
    char cpu_brand[49];
    char display_type[16];
    int floppy_drives, serial_ports;
    unsigned int conv_memory, ext_memory;
    int hours, minutes, seconds, day, month, year;
    uint16_t equipment;
    uint8_t display_mode;
    
    // Gather system information
    get_cpu_vendor(cpu_vendor);
    get_cpu_brand(cpu_brand);
    conv_memory = get_conventional_memory();
    ext_memory = get_extended_memory();
    equipment = get_equipment_word();
    parse_equipment(equipment, display_type, &floppy_drives, &serial_ports);
    display_mode = get_display_mode();
    get_rtc_time(&hours, &minutes, &seconds);
    get_rtc_date(&day, &month, &year);
    
    vga_println("=== LemonOS System Information ===");
    vga_println("");
    
    // OS Information
    vga_println("OS Information:");
    vga_println("  Name: LemonOS");
    vga_println("  Version: 0.1");
    vga_println("  Developer: Dwip");
    vga_println("  Architecture: 32-bit x86");
    vga_println("");
    
    // CPU Information
    vga_println("CPU Information:");
    vga_print("  Vendor: ");
    vga_println(cpu_vendor);
    vga_print("  Brand: ");
    vga_println(cpu_brand);
    vga_println("");
    
    // Memory Information
    vga_println("Memory Information:");
    vga_print("  Conventional: ");
    vga_print_int(conv_memory);
    vga_println(" KB");
    vga_print("  Extended: ");
    vga_print_int(ext_memory);
    vga_println(" KB");
    vga_print("  Total: ");
    vga_print_int(conv_memory + ext_memory);
    vga_println(" KB");
    vga_println("");
    
    // Hardware Information
    vga_println("Hardware Information:");
    vga_print("  Display: ");
    vga_println(display_type);
    vga_print("  Display Mode: 0x");
    vga_print_hex(display_mode);
    vga_println("");
    vga_print("  Floppy Drives: ");
    vga_print_int(floppy_drives);
    vga_println("");
    vga_print("  Serial Ports: ");
    vga_print_int(serial_ports);
    vga_println("");
    vga_println("");
    
    // Time Information
    vga_println("Time Information:");
    vga_print("  Date: ");
    vga_print_int(day);
    vga_putchar('/');
    vga_print_int(month);
    vga_putchar('/');
    vga_print_int(year);
    vga_println("");
    
    vga_print("  Time: ");
    if (hours < 10) vga_putchar('0');
    vga_print_int(hours);
    vga_putchar(':');
    if (minutes < 10) vga_putchar('0');
    vga_print_int(minutes);
    vga_putchar(':');
    if (seconds < 10) vga_putchar('0');
    vga_print_int(seconds);
    vga_println("");
    vga_println("");
    
    vga_println("Press any key to continue...");
    
    // Wait for key press
    while(1) {
        poll_keyboard();
        if (has_key()) {
            getchar();
            break;
        }
    }
}
