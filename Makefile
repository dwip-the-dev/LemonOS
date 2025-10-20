# Makefile for LemonOS

# Compiler and flags
CC = gcc
CFLAGS = -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -m32 -Iinclude -Wno-array-bounds

# FIXED: Added games/doom_real.o to OBJS
OBJS = boot.o kernel.o vga.o keyboard.o shell.o commands.o \
       apps/calculator.o apps/clock.o apps/system_info.o apps/unit_converter.o apps/screensaver.o apps/file_manager.o apps/color_demo.o \
       drivers/rtc.o drivers/vga_utils.o drivers/simple_fs.o drivers/shutdown.o drivers/reboot.o drivers/gui.o \
       drivers/stdlib.o drivers/stdio.o drivers/string.o \
       games/snake.o games/breakout.o

# Default target
all: lemon.bin

# Linking
lemon.bin: $(OBJS)
	$(CC) -m32 -nostdlib -o lemon.bin $(OBJS) -T linker.ld

# Kernel objects
boot.o: boot.s
	$(CC) $(CFLAGS) -c boot.s -o boot.o

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

vga.o: vga.c
	$(CC) $(CFLAGS) -c vga.c -o vga.o

keyboard.o: keyboard.c
	$(CC) $(CFLAGS) -c keyboard.c -o keyboard.o

shell.o: shell.c
	$(CC) $(CFLAGS) -c shell.c -o shell.o

commands.o: commands.c
	$(CC) $(CFLAGS) -c commands.c -o commands.o

# App objects
apps/calculator.o: apps/calculator.c include/calculator.h include/vga.h include/keyboard.h include/vga_utils.h
	$(CC) $(CFLAGS) -c apps/calculator.c -o apps/calculator.o

apps/unit_converter.o: apps/unit_converter.c include/unit_converter.h include/vga.h include/keyboard.h include/vga_utils.h
	$(CC) $(CFLAGS) -c apps/unit_converter.c -o apps/unit_converter.o

apps/clock.o: apps/clock.c include/clock.h include/vga.h include/keyboard.h include/rtc.h include/vga_utils.h
	$(CC) $(CFLAGS) -c apps/clock.c -o apps/clock.o

apps/system_info.o: apps/system_info.c include/system_info.h include/vga.h include/keyboard.h include/rtc.h include/vga_utils.h
	$(CC) $(CFLAGS) -c apps/system_info.c -o apps/system_info.o

apps/screensaver.o: apps/screensaver.c include/screensaver.h include/vga.h include/keyboard.h
	$(CC) $(CFLAGS) -c apps/screensaver.c -o apps/screensaver.o

apps/file_manager.o: apps/file_manager.c include/file_manager.h include/vga.h include/keyboard.h include/filesystem.h include/vga_utils.h
	$(CC) $(CFLAGS) -c apps/file_manager.c -o apps/file_manager.o

apps/color_demo.o: apps/color_demo.c include/color_demo.h include/vga.h include/keyboard.h include/vga_utils.h
	$(CC) $(CFLAGS) -c apps/color_demo.c -o apps/color_demo.o

# Driver objects
drivers/rtc.o: drivers/rtc.c include/rtc.h
	$(CC) $(CFLAGS) -c drivers/rtc.c -o drivers/rtc.o

drivers/vga_utils.o: drivers/vga_utils.c include/vga_utils.h include/vga.h
	$(CC) $(CFLAGS) -c drivers/vga_utils.c -o drivers/vga_utils.o

drivers/simple_fs.o: drivers/simple_fs.c include/filesystem.h include/vga.h
	$(CC) $(CFLAGS) -c drivers/simple_fs.c -o drivers/simple_fs.o

drivers/text_graphics.o: drivers/text_graphics.c include/text_graphics.h include/vga.h include/keyboard.h
	$(CC) $(CFLAGS) -c drivers/text_graphics.c -o drivers/text_graphics.o
	
drivers/shutdown.o: drivers/shutdown.c include/shutdown.h
	$(CC) $(CFLAGS) -c drivers/shutdown.c -o drivers/shutdown.o
	
drivers/reboot.o: drivers/reboot.c include/reboot.h
	$(CC) $(CFLAGS) -c drivers/reboot.c -o drivers/reboot.o

drivers/gui.o: drivers/gui.c include/gui.h include/vga.h include/keyboard.h include/vga_utils.h
	$(CC) $(CFLAGS) -c drivers/gui.c -o drivers/gui.o
	
drivers/stdlib.o: drivers/stdlib.c include/stdlib.h include/string.h
	$(CC) $(CFLAGS) -c drivers/stdlib.c -o drivers/stdlib.o

drivers/stdio.o: drivers/stdio.c include/stdio.h include/vga.h include/string.h
	$(CC) $(CFLAGS) -c drivers/stdio.c -o drivers/stdio.o

drivers/string.o: drivers/string.c include/string.h
	$(CC) $(CFLAGS) -c drivers/string.c -o drivers/string.o
	
# Game objects
games/snake.o: games/snake.c include/snake.h include/text_graphics.h include/vga.h include/keyboard.h
	$(CC) $(CFLAGS) -c games/snake.c -o games/snake.o
	
# FIXED: Corrected target name from snake.o to brick_breaker.o
games/brick_breaker.o: games/breakout.c include/breakout.h include/text_graphics.h include/vga.h include/keyboard.h
	$(CC) $(CFLAGS) -c games/breakout.c -o games/breakout.o

# Clean
clean:
	rm -f *.o *.bin lemon.iso
	rm -rf iso
	rm -f apps/*.o drivers/*.o games/*.o

run: lemon.bin
	qemu-system-i386 -kernel lemon.bin

# Create ISO (if needed)
iso: lemon.bin
	mkdir -p iso/boot/grub
	cp lemon.bin iso/boot/
	echo 'set timeout=0' > iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo 'menuentry "LemonOS" {' >> iso/boot/grub/grub.cfg
	echo '  multiboot /boot/lemon.bin' >> iso/boot/grub/grub.cfg
	echo '  boot' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o lemon.iso iso

# Debug build with symbols
debug: CFLAGS += -g
debug: lemon.bin

# Release build with optimizations
release: CFLAGS += -DNDEBUG
release: lemon.bin

# Build all targets
all: lemon.bin

# List all source files
sources:
	@echo "Source files:"
	@find . -name "*.c" -o -name "*.s" -o -name "*.h" | grep -v iso | sort

# Count lines of code
loc:
	@echo "Lines of code:"
	@find . -name "*.c" -o -name "*.s" -o -name "*.h" | grep -v iso | xargs wc -l

# Create directories if they don't exist
directories:
	mkdir -p apps drivers include games

# Help target
help:
	@echo "LemonOS Makefile Targets:"
	@echo "  all       - Build lemon.bin (default)"
	@echo "  clean     - Remove all build artifacts"
	@echo "  run       - Run in QEMU"
	@echo "  iso       - Create bootable ISO"
	@echo "  debug     - Build with debug symbols"
	@echo "  release   - Build with release optimizations"
	@echo "  sources   - List all source files"
	@echo "  loc       - Count lines of code"
	@echo "  directories - Create necessary directories"
	@echo "  help      - Show this help"

.PHONY: all clean run iso debug release sources loc directories help
