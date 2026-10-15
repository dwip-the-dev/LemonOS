# LemonOS Development & Contributor Guide

**Author:** Dwip Biswas ([@dwip-the-dev](https://github.com/dwip-the-dev))  
**Target Architecture:** x86-32 (i386+)  

---

## 1. Toolchain & Prerequisites

### Linux (Arch Linux / CachyOS)
```bash
sudo pacman -S gcc-multilib nasm qemu-system-x86 grub xorriso mtools
```

### Linux (Debian / Ubuntu)
```bash
sudo apt update
sudo apt install build-essential gcc-multilib nasm qemu-system-x86 grub-pc-bin grub-common xorriso mtools
```

### macOS (Homebrew)
```bash
brew install i686-elf-gcc nasm qemu xorriso
```

---

## 2. Building & Running

### Full Build
```bash
make clean && make
```
Generates `lemon.bin`, a Multiboot-compliant kernel image.

### Building Bootable ISO
```bash
make iso
```
Creates `lemon.iso` bootable with GNU GRUB on VMs (QEMU, Bochs, VirtualBox, VMware) or real hardware via USB (`dd if=lemon.iso of=/dev/sdX bs=4M status=progress`).

### Running in QEMU
```bash
# Standard graphical desktop run:
qemu-system-i386 -cdrom lemon.iso -m 128M -vga std -net nic,model=e1000 -net user

# Or directly boot kernel binary:
make run
```

---

## 3. Writing Userspace Programs

LemonOS provides a complete POSIX C99 development environment:

1. Create a C file in `user/` (e.g. `user/myprogram.c`):
```c
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    printf("Hello from LemonOS userspace! PID=%d\n", getpid());
    return 0;
}
```

2. Compile against `libc/`:
```bash
gcc -m32 -nostdlib -fno-builtin -Ilibc/include -c user/myprogram.c -o user/myprogram.o
gcc -m32 -nostdlib -no-pie -T libc/user.ld libc/crt0.o user/myprogram.o libc/libc.a -o user/myprogram.elf
```

3. Update `scripts/gen_elf_bins.py` and run `python3 scripts/gen_elf_bins.py` to embed the binary directly into the root filesystem.

---

## 4. Package Manager (`.lem`)

LemonOS includes its own native `.lem` package format:
- Manifest structure with metadata (`Package`, `Version`, `Description`, `Architecture`).
- Automatic MD5 / SHA-256 checksum verification.
- Post-install script hooks and `.desktop` application menu integration.
