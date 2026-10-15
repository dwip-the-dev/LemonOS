#include <stdint.h>
#include <stddef.h>
#include "vga.h"
#include "keyboard.h"
#include "shell.h"
#include "bga.h"
#include "desktop.h"
#include "serial.h"
#include "gdt.h"
#include "idt.h"
#include "syscall.h"
#include "user_loader.h"
#include "user_programs.h"
#include "block.h"
#include "ata.h"
#include "ramdisk.h"
#include "vfs.h"
#include "fs_lemon.h"
#include "devfs.h"
#include "filesystem.h"
#include "pci.h"
#include "net.h"
#include "dynlink.h"
#include "crypto.h"
#include "entropy.h"
#include "posix_time.h"
#include "audio.h"
#include "inotify.h"
#include "mman.h"
#include "termios.h"
#include "proc_mgr.h"
#include "service.h"
#include "procfs.h"
#include "compiler.h"
#include "make.h"
#include "git.h"
#include "apt.h"
#include "init.h"
#include "auth.h"
#include "langrt.h"
#include "fast_mem.h"
#include "slab.h"
#include "bcache.h"
#include "pipe.h"
#include "pmm.h"
#include "vmm.h"
#include "kmem.h"
#include "sync.h"
#include "signal.h"
#include "shm.h"

void kernel_main(void) {
    // Initialize serial for logging
    serial_init();
    serial_println("=================================");
    serial_println("[LemonOS] Kernel main reached!");
    serial_println("=================================");

    // Initialize hardware
    vga_initialize();
    serial_println("[LemonOS] VGA initialized.");
    keyboard_init();
    serial_println("[LemonOS] Keyboard initialized.");

    // Initialize GDT and Task State Segment (TSS) for Ring 3 User Mode
    gdt_init();

    // Initialize IDT and Syscall Vector 0x80
    idt_init();

    // Initialize Core Physical & Virtual Memory Managers (Hardware Paging, 1 GB RAM)
    pmm_init(1024 * 1024);
    pmm_run_tests();
    vmm_init();
    vmm_run_tests();

    // Initialize Dynamic Kernel Heap & Slab Allocator (kmalloc / kfree)
    kmem_init();
    kmem_run_tests();

    // Initialize Core Kernel Synchronization (Atomics, Hardware Spinlocks, Mutexes)
    sync_init();
    sync_run_tests();

    // Initialize System API (Syscall Dispatcher)
    syscall_init();

    // Initialize Userspace Process Loader & Dynamic Linker (GOT/PLT Engine)
    user_loader_init();
    dynlink_init();

    // Initialize C++ Core Kernel Slab Allocator & Object Pool
    slab_init();
    slab_run_tests();

    // Initialize Block Device Subsystem & Drivers (ATA/IDE & Ramdisk)
    block_init();
    ata_init();
    ramdisk_init();
    block_run_tests();
    bcache_run_tests();

    // Initialize Virtual File System (VFS) and UNIX Pipe Subsystem
    vfs_init();
    pipe_init();
    pipe_run_tests();

    // Format & Mount Root Block Filesystem (LemonFS)
    block_dev_t* default_disk = block_get_default();
    if (default_disk) {
        vfs_node_t* fs_root = fs_lemon_mount(default_disk);
        if (fs_root) {
            vfs_mount("/", fs_root);
        }
        fs_lemon_run_tests(default_disk);
    }

    // Mount Virtual Device Filesystem (/dev)
    devfs_init();

    // Run VFS Test Suite
    vfs_run_tests();

    // Initialize LemonFS Directory Hierarchy and Default Documentation Files
    fs_init();
    serial_println("[LemonOS] LemonFS initialized.");
    fs_run_tests();

    // Initialize User Authentication & Privilege System
    auth_init();
    serial_println("[LemonOS] Auth subsystem ready.");

    // Initialize POSIX Time & Core Process Manager, Shared Memory IPC, and Signals
    posix_time_init();
    posix_time_run_tests();
    audio_init();
    audio_run_tests();
    proc_mgr_init();
    proc_mgr_run_tests();
    shm_init();
    shm_run_tests();
    signal_init();
    signal_run_tests();

    // Install Built-in Userspace Applications to /bin
    user_programs_install();

    // Run Syscall & Userspace Loader Verification Suites
    syscall_run_tests();
    user_loader_run_tests();
    dynlink_run_tests();

    // Initialize PCI Bus & Network Stack
    pci_init();
    net_init();
    serial_println("[LemonOS] Network stack initialized.");
    net_run_tests();

    // Initialize Cryptographic Hashing, Entropy & Terminal Control
    crypto_run_tests();
    entropy_init();
    entropy_run_tests();
    inotify_init_subsystem();
    inotify_run_tests();
    mman_init();
    mman_run_tests();
    termios_init();
    termios_run_tests();

    // Initialize Service Daemons & Init System
    service_init_subsystem();
    service_run_tests();
    init_system_init();
    init_system_boot();
    init_run_tests();

    // Mount Process Filesystem (/proc)
    procfs_init();
    procfs_run_tests();

    // Initialize Developer Toolchain Suite (GCC, Clang, Make, Git, APT)
    compiler_init();
    compiler_run_tests();
    langrt_init();
    langrt_run_tests();
    make_init();
    make_run_tests();
    git_init_subsystem();
    git_run_tests();
    apt_init_subsystem();
    apt_run_tests();

    vga_println("Booting LemonOS...");

    // Check graphics adapter
    serial_println("[LemonOS] Checking BGA availability...");
    if (bga_is_available()) {
        serial_println("[LemonOS] BGA detected! Launching Desktop GUI...");
        desktop_run();
        serial_println("[LemonOS] Desktop GUI exited.");
    } else {
        serial_println("[LemonOS] BGA not detected. Using VGA text mode.");
    }

    // Run the shell loop (upon exiting GUI or if no graphics adapter)
    vga_clear();
    vga_println("LemonOS CLI Console (type 'gui' to start Desktop)");
    shell_run();

    // Should never return
    while (1) {}
}
