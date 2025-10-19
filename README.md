# 🍋 LemonOS

> *A zesty, baremetal operating system from scratch with a refreshing twist*

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/dwip-the-dev/LemonOS)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: x86](https://img.shields.io/badge/platform-x86--32-blue)](https://github.com/dwip-the-dev/LemonOS)

A feature-packed, from-scratch 32-bit x86 operating system written in C and Assembly. LemonOS proves that baremetal development can be both educational and deliciously fun! Based on my long abandoned project NesOS.

## ⚠️THIS OS IS NOT MEANT TO BE RUN ON A REAL SYSTEM! THIS IS ONLY TO BE RUN IN A VM.

## 🚀 Features

### 🛠️ Core System
- **Custom Bootloader** - Handcrafted Multiboot-compliant boot sector
- **Protected Mode** - Full 32-bit protected mode with GDT
- **Memory Management** - Basic memory detection and management
- **VGA Text Driver** - Complete VGA text mode implementation
- **PS/2 Keyboard** - Full keyboard driver with input buffering

### 🎮 Built-in Applications
| App | Description | Cool Factor |
|-----|-------------|-------------|
| 🧮 **Calculator** | Full arithmetic operations with decimal support | ✅ |
| ⏰ **RTC Clock** | Real-time clock with hardware RTC access | ✅ |
| 🖥️ **System Info** | CPU, memory, and hardware detection | ✅ |
| 🔄 **Unit Converter** | Temperature, distance, weight, volume, and more | ✅ |
| 🌧️ **Matrix Screensaver** | Matrix Effect | 🚀 |
| 💻 **Interactive Shell** | Command-line interface with tab completion | ✅ |

### 🎨 Visual Flair
- **ASCII Art Boot Screen** - Beautiful lemon-themed artwork
- **Matrix Screensaver** - Activates after 60 seconds of inactivity
- **Real-time Clock** - Hardware RTC integration for accurate time
- **CLI Interface** - Clean, professional terminal interface


```
🍋 LemonOS v0.1 - Baremetal OS by YourName

LemonOS> help
Commands: help, clear, about, calc, clock, info, convert, matrix
  help    - Show this help message
  clear   - Clear the screen  
  about   - Show system information
  calc    - Simple calculator
  clock   - Real-time clock with RTC
  info    - Detailed system information
  convert - Unit converter
  matrix  - Start Matrix screen saver manually

```

## 🚀 Quick Start

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get install gcc-multilib qemu-system-x86 grub-pc-bin

# Arch Linux  
sudo pacman -S gcc-multilib qemu grub

# macOS (with Homebrew)
brew install x86_64-elf-gcc qemu
```

### Build & Run
```bash
# Clone and build
git clone https://github.com/dwip-the-dev/LemonOS.git
cd LemonOS
make clean && make

# Run in QEMU
make run

# Create bootable ISO
make iso
```

### Available Make Targets
```bash
make           # Build LemonOS
make run       # Run in QEMU
make iso       # Create bootable ISO
make clean     # Clean build artifacts
make debug     # Build with debug symbols
```

## 🔬 Technical Details

### Boot Process
1. **BIOS** loads boot sector
2. **Bootloader** enters protected mode
3. **Kernel** initializes hardware
4. **Shell** starts interactive session

### Hardware Support
- **CPU**: x86-32 (i386+)
- **Memory**: Conventional + Extended
- **Display**: VGA Text Mode (80x25)
- **Input**: PS/2 Keyboard
- **Time**: CMOS RTC

## 🏗️ Project Structure

```
LemonOS
├── apps
│   ├── calculator.c
│   ├── clock.c
│   ├── screensaver.c
│   ├── system_info.c
│   └── unit_converter.c
├── art.h
├── boot.s
├── commands.c
├── commands.h
├── drivers
│   ├── rtc.c
│   └── vga_utils.c
├── grub.cfg
├── include
│   ├── calculator.h
│   ├── clock.h
│   ├── keyboard.h
│   ├── rtc.h
│   ├── screensaver.h
│   ├── stddef.h
│   ├── stdint.h
│   ├── system_info.h
│   ├── unit_converter.h
│   ├── vga.h
│   └── vga_utils.h
├── kernel.c
├── keyboard.c
├── linker.ld
├── Makefile
├── README.md
├── shell.c
├── shell.h
└── vga.c

4 directories, 31 files
```

## 🤝 Contributing

Found a bug? Want a new feature? Contributions are welcome!

1. Fork the repository
2. Create a feature branch (`git checkout -b amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin amazing-feature`)
5. Open a Pull Request

### Ideas for Contribution
- [ ] File system support
- [ ] Simple text editor
- [ ] Basic games (Snake, Pong)
- [ ] Network stack
- [ ] More Apps
- [ ] Run DOOM
- [ ] Sound driver

## 📚 Learning Resources

This project is excellent for learning:
- **Operating System Design**
- **x86 Assembly Programming**
- **Hardware Interaction**
- **System Programming**
- **Boot Process & Firmware**

## 🙏 Acknowledgments

- **OSDev.org** - Invaluable community and wiki
- **James Molloy's OS Tutorial** - Foundational knowledge
- **Multiboot Specification** - Boot standard
- **All open-source OS projects** - Inspiration and reference

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

**🍋 Made with fresh-squeezed code and no artificial preservatives**

*If you find this project helpful, please give it a ⭐!*
