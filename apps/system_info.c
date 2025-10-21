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

unsigned int get_extended_memory(void) {

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

// Helper function to print colored value
static void print_colored_value(const char* label, const char* value, uint8_t label_color, uint8_t value_color) {
    vga_print_color("  ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print_color(label, label_color, VGA_COLOR_BLACK);
    vga_print_color(": ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print_color(value, value_color, VGA_COLOR_BLACK);
    vga_println_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

// Helper function to print colored numeric value
static void print_colored_number(const char* label, int value, uint8_t label_color, uint8_t value_color) {
    vga_print_color("  ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print_color(label, label_color, VGA_COLOR_BLACK);
    vga_print_color(": ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Manual number printing with color
    if (value == 0) {
        vga_putchar_color('0', value_color, VGA_COLOR_BLACK);
    } else {
        char temp[20];
        int temp_pos = 0;
        int num = value;
        
        while (num > 0) {
            temp[temp_pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (temp_pos > 0) {
            vga_putchar_color(temp[--temp_pos], value_color, VGA_COLOR_BLACK);
        }
    }
    vga_println_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
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
    
    vga_clear();
    
    // Header
    vga_println_color("=== LemonOS System Information ===", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_println_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // OS Information
    vga_println_color("OS Information:", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    print_colored_value("Name", "LemonOS", VGA_COLOR_LIGHT_GREY, VGA_COLOR_GREEN);
    print_colored_value("Version", "0.1", VGA_COLOR_LIGHT_GREY, VGA_COLOR_GREEN);
    print_colored_value("Developer", "Dwip", VGA_COLOR_LIGHT_GREY, VGA_COLOR_GREEN);
    print_colored_value("Architecture", "32-bit x86", VGA_COLOR_LIGHT_GREY, VGA_COLOR_GREEN);
    vga_println_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // CPU Information
    vga_println_color("CPU Information:", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    print_colored_value("Vendor", cpu_vendor, VGA_COLOR_LIGHT_GREY, VGA_COLOR_LIGHT_BLUE);
    print_colored_value("Brand", cpu_brand, VGA_COLOR_LIGHT_GREY, VGA_COLOR_LIGHT_BLUE);
    vga_println_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Memory Information
    vga_println_color("Memory Information:", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    
    // Conventional memory
    vga_print_color("  Conventional: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_print_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK); // Reset color
    // Print number manually with color
    if (conv_memory == 0) {
        vga_putchar_color('0', VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    } else {
        char temp[20];
        int temp_pos = 0;
        unsigned int num = conv_memory;
        
        while (num > 0) {
            temp[temp_pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (temp_pos > 0) {
            vga_putchar_color(temp[--temp_pos], VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        }
    }
    vga_println_color(" KB", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Extended memory
    vga_print_color("  Extended: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_print_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    if (ext_memory == 0) {
        vga_putchar_color('0', VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    } else {
        char temp[20];
        int temp_pos = 0;
        unsigned int num = ext_memory;
        
        while (num > 0) {
            temp[temp_pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (temp_pos > 0) {
            vga_putchar_color(temp[--temp_pos], VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        }
    }
    vga_println_color(" KB", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Total memory
    vga_print_color("  Total: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_print_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    unsigned int total_memory = conv_memory + ext_memory;
    if (total_memory == 0) {
        vga_putchar_color('0', VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    } else {
        char temp[20];
        int temp_pos = 0;
        unsigned int num = total_memory;
        
        while (num > 0) {
            temp[temp_pos++] = '0' + (num % 10);
            num /= 10;
        }
        while (temp_pos > 0) {
            vga_putchar_color(temp[--temp_pos], VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        }
    }
    vga_println_color(" KB", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Hardware Information
    vga_println_color("Hardware Information:", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    print_colored_value("Display", display_type, VGA_COLOR_LIGHT_GREY, VGA_COLOR_MAGENTA);
    
    // Display mode with hex
    vga_print_color("  Display Mode: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_print_color("0x", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Print hex manually with color
    uint8_t mode = display_mode;
    char hex_chars[] = "0123456789ABCDEF";
    vga_putchar_color(hex_chars[(mode >> 4) & 0x0F], VGA_COLOR_MAGENTA, VGA_COLOR_BLACK);
    vga_putchar_color(hex_chars[mode & 0x0F], VGA_COLOR_MAGENTA, VGA_COLOR_BLACK);
    vga_println_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    print_colored_number("Floppy Drives", floppy_drives, VGA_COLOR_LIGHT_GREY, VGA_COLOR_MAGENTA);
    print_colored_number("Serial Ports", serial_ports, VGA_COLOR_LIGHT_GREY, VGA_COLOR_MAGENTA);
    vga_println_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Time Information
    vga_println_color("Time Information:", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    
    // Date
    vga_print_color("  Date: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Print day
    if (day < 10) vga_putchar_color('0', VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    print_colored_number("", day, VGA_COLOR_LIGHT_GREY, VGA_COLOR_LIGHT_RED);
    vga_print_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK); // Reset for separator
    vga_putchar_color('/', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Print month
    if (month < 10) vga_putchar_color('0', VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    print_colored_number("", month, VGA_COLOR_LIGHT_GREY, VGA_COLOR_LIGHT_RED);
    vga_print_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_putchar_color('/', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Print year
    print_colored_number("", year, VGA_COLOR_LIGHT_GREY, VGA_COLOR_LIGHT_RED);
    vga_println_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Time
    vga_print_color("  Time: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Print hours
    if (hours < 10) vga_putchar_color('0', VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    print_colored_number("", hours, VGA_COLOR_LIGHT_GREY, VGA_COLOR_LIGHT_RED);
    vga_print_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_putchar_color(':', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Print minutes
    if (minutes < 10) vga_putchar_color('0', VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    print_colored_number("", minutes, VGA_COLOR_LIGHT_GREY, VGA_COLOR_LIGHT_RED);
    vga_print_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_putchar_color(':', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Print seconds
    if (seconds < 10) vga_putchar_color('0', VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    print_colored_number("", seconds, VGA_COLOR_LIGHT_GREY, VGA_COLOR_LIGHT_RED);
    vga_println_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    vga_println_color("Press any key to continue...", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Wait for key press
    while(1) {
        poll_keyboard();
        if (has_key()) {
            getchar();
            break;
        }
    }
    
    // Reset to default colors
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}
