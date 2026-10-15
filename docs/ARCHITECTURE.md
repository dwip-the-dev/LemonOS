# LemonOS Kernel Architecture & Subsystems

**Author:** Dwip Biswas ([@dwip-the-dev](https://github.com/dwip-the-dev))  
**Target Architecture:** x86-32 (IA-32 / i386+)  
**Version:** Alpha 1.0  
**License:** MIT  

---

## 1. Overview & Boot Sequence

LemonOS is a baremetal 32-bit x86 operating system adhering to the Multiboot specification (Multiboot 1). The boot sequence proceeds as follows:

```
+-------------------------------------------------------+
|  BIOS / UEFI CSM (Real Mode 16-bit)                   |
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
|  GNU GRUB Bootloader (boot.s)                         |
|  - Validates Multiboot Magic (0x1BADB002)             |
|  - Sets up initial stack (16 KB)                      |
|  - Passes Multiboot Info structure pointer in EBX     |
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
|  Kernel Entry Point: kernel_early_init() & kernel_main|
|  1. GDT & Segment Selectors (drivers/gdt.c)          |
|  2. IDT & Exception Handlers 0-31 (drivers/idt.c)     |
|  3. PIC 8259 Remap (IRQs 32-47)                       |
|  4. Hardware Timer / PIT Calibration (RDTSC / 1000Hz) |
|  5. PMM (Bitmap Frame Allocator)                      |
|  6. VMM (Two-Level Page Directories & Page Tables)    |
|  7. Kernel Heap & SLAB Allocator (kmem)               |
|  8. VFS & Root Mount Points (LemonFS, DevFS, ProcFS)  |
|  9. PCI Bus & Hardware Probing (e1000, BGA, ATA)      |
| 10. Process Manager & POSIX Syscall Dispatcher        |
| 11. Graphical Desktop & Window Manager / Shell Launch |
+-------------------------------------------------------+
```

---

## 2. Memory Management

LemonOS implements a three-tier memory architecture separating physical allocation, virtual memory paging, and kernel slab caching.

### Physical Memory Manager (PMM)
- **File:** `drivers/pmm.c`, `include/pmm.h`
- **Implementation:** Frame allocation bitmap where each bit represents a 4096-byte (4 KB) page frame.
- **Physical Detection:** CMOS register probing (0x30/0x31) combined with Multiboot memory map traversal.
- **Functions:** `pmm_alloc_frame()`, `pmm_free_frame()`, `pmm_get_total_frames()`, `pmm_get_free_frames()`.

### Virtual Memory Manager (VMM) & Paging
- **File:** `drivers/vmm.c`, `include/vmm.h`
- **Model:** x86 Two-Level Paging (Page Directory + Page Tables) with 4 KB granularity.
- **Address Space Layout:**
  - `0x00000000 - 0x000FFFFF`: Real Mode IVT, BIOS data area, VGA buffers (Identity mapped).
  - `0x00100000 - 0x01FFFFFF`: Kernel Code, Data, BSS, Heap, and Stack.
  - `0x02000000 - 0xBFFFFFFF`: Ring 3 Userspace Process Space (Code, Data, Stack).
  - `0xC0000000 - 0xFFFFFFFF`: High-memory mappings, PCI MMIO buffers, Framebuffer aperture.
- **Fault Handling:** Page Fault Exception (ISR 14) resolves on-demand stack growth and copy-on-write mappings.

### Kernel Heap & SLAB Allocator
- **File:** `drivers/kmem.c`, `drivers/slab.cpp`, `include/kmem.h`, `include/slab.h`
- **SLAB Cache:** Cache bins for frequent small kernel structures (descriptors, nodes, process control blocks).
- **Heap Allocator:** Boundary-tag segregated free list supporting arbitrary `kmalloc()` and `kfree()`.

---

## 3. Process Management & Multitasking

LemonOS supports preemptive multitasking with Ring 3 userspace isolation:

- **Process Control Block (PCB):** Tracks PID, PPID, state (RUNNING, READY, BLOCKED, ZOMBIE), register state (context frame), virtual memory page directory, file descriptor table, and working directory (`cwd`).
- **Context Switching:** Hardware interrupt timer (IRQ0) triggers scheduler; CPU state saved to kernel stack, TSS updated, page directory switched (CR3 register), and registers restored.
- **POSIX Fork & Exec:**
  - `sys_fork()`: Clones parent PCB, duplicates page tables, copies user space pages, and returns child PID to parent, 0 to child.
  - `sys_waitpid()`: Blocks parent process until child terminates and collects exit code.

---

## 4. Virtual File System (VFS)

The VFS provides a unified POSIX file descriptor abstraction across multiple storage backends:

```
                     +----------------------------+
                     |    POSIX Syscall Layer     |
                     | read, write, open, close   |
                     | dup, dup2, fcntl, ioctl    |
                     +----------------------------+
                                   |
                                   v
+----------------------------------------------------------------------+
|                     Virtual File System (VFS)                        |
|                     drivers/vfs.c, include/vfs.h                     |
+----------------------------------------------------------------------+
         |                   |                    |                |
         v                   v                    v                v
+------------------+ +-----------------+ +-----------------+ +-------------+
|     LemonFS      | |      DevFS      | |     ProcFS      | |   Ramdisk   |
| (Root Filesystem)| | /dev/tty        | | /proc/cpuinfo   | | Fast RAM FS |
| Simple & block-  | | /dev/console    | | /proc/meminfo   | | storage     |
| backed storage   | | /dev/null,/zero | | /proc/uptime    | |             |
+------------------+ +-----------------+ +-----------------+ +-------------+
```

### File Descriptor Redirection
File descriptors 0 (`stdin`), 1 (`stdout`), and 2 (`stderr`) can be dynamically redirected to files or IPC pipes using `vfs_dup()` and `vfs_dup2()`. System calls `SYS_READ` and `SYS_WRITE` honor active VFS file descriptors before falling back to the console devices.

---

## 5. Device Drivers

| Driver | File | Description |
|--------|------|-------------|
| **BGA / Graphics** | `drivers/bga.c`, `drivers/graphics.c` | Bochs/QEMU VBE Extensions supporting 1024x768 32-bit truecolor framebuffer. |
| **Intel e1000** | `drivers/e1000.c` | PCI Intel 82540EM Gigabit Ethernet NIC with DMA descriptor rings. |
| **ATA / IDE** | `drivers/ata.c`, `drivers/block.c` | PIO mode 28/48-bit LBA hard disk controller with block cache (`bcache.c`). |
| **Serial COM1** | `drivers/serial.c` | UART 16550 serial port at 0x3F8 for debugging and headless automation. |
| **PS/2 Keyboard & Mouse** | `keyboard.c`, `drivers/mouse.c` | Interrupt-driven scancode translation and 3-button packet decoding. |
| **CMOS / RTC** | `drivers/rtc.c`, `drivers/posix_time.c` | Hardware real-time clock with BCD decoding and microsecond RDTSC calibration. |
| **Audio** | `drivers/audio.c` | PC Speaker tone generation and Sound Blaster 16 DSP abstraction. |
