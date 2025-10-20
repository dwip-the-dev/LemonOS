#include <stdint.h>
#include <stddef.h>
#include "vga.h"
#include "keyboard.h"
#include "shell.h"


void kernel_main(void) {
    // Clear sceen and boot banner
    vga_clear();
    vga_println("Booting LemonOS...");

    // Initialize hardware
    keyboard_init();
    vga_initialize();

    // Run the shell loop
    shell_run();

    // Should never return
    while (1) {}
}
