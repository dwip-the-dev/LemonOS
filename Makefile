# Makefile for LemonOS

# Compiler and flags
CC = gcc
CFLAGS = -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -m32 -Iinclude -Wno-array-bounds

OBJS = boot.o kernel.o vga.o keyboard.o shell.o commands.o \
       apps/calculator.o apps/clock.o apps/system_info.o apps/unit_converter.o apps/screensaver.o apps/file_manager.o apps/color_demo.o \
       apps/gui_firefox.o apps/gui_terminal.o apps/gui_calc.o apps/gui_converter.o apps/gui_sysinfo.o apps/gui_files.o apps/gui_clock.o \
       apps/gui_snake.o apps/gui_breakout.o apps/gui_pong.o apps/gui_matrix.o apps/gui_settings.o apps/gui_services.o apps/gui_speedtest.o \
       drivers/bga.o drivers/mouse.o drivers/font.o drivers/icons.o drivers/icon_data.o drivers/graphics.o drivers/gui_engine.o drivers/wm.o drivers/desktop.o drivers/alerts.o \
       drivers/netsurf.o \
       drivers/pci.o drivers/e1000.o drivers/net.o drivers/tls.o \
       drivers/block.o drivers/bcache.o drivers/ata.o drivers/ramdisk.o drivers/vfs.o drivers/pipe.o drivers/devfs.o drivers/fs_lemon.o \
       drivers/pmm.o drivers/vmm.o drivers/kmem.o drivers/sync.o drivers/signal.o drivers/shm.o \
       drivers/sys_asm.o drivers/fast_mem.o drivers/slab.o drivers/gdt.o drivers/idt.o drivers/syscall.o drivers/user_loader.o drivers/dynlink.o apps/user_programs.o \
       drivers/crypto.o drivers/entropy.o drivers/posix_time.o drivers/audio.o drivers/inotify.o drivers/mman.o drivers/termios.o \
       drivers/proc_mgr.o drivers/service.o drivers/init.o drivers/procfs.o drivers/compiler.o drivers/langrt.o drivers/make.o drivers/git.o drivers/apt.o drivers/auth.o \
       drivers/rtc.o drivers/vga_utils.o drivers/simple_fs.o drivers/shutdown.o drivers/reboot.o drivers/gui.o \
       drivers/stdlib.o drivers/stdio.o drivers/string.o drivers/serial.o \
       apps/nano.o apps/vim.o apps/less.o apps/lua.o apps/sqlite.o apps/busybox.o apps/tar_gzip.o apps/ssh.o apps/curl.o apps/speedtest.o \
       games/snake.o games/breakout.o games/pong.o

DOOM_OBJS = games/doom/am_map.o games/doom/d_event.o games/doom/d_items.o games/doom/d_iwad.o games/doom/d_loop.o \
            games/doom/d_main.o games/doom/d_mode.o games/doom/d_net.o games/doom/doomdef.o games/doom/doomgeneric.o \
            games/doom/doomgeneric_lemon.o games/doom/doomstat.o games/doom/dstrings.o games/doom/dummy.o \
            games/doom/f_finale.o games/doom/f_wipe.o games/doom/g_game.o games/doom/gusconf.o games/doom/hu_lib.o \
            games/doom/hu_stuff.o games/doom/i_cdmus.o games/doom/i_endoom.o games/doom/i_input.o games/doom/i_joystick.o \
            games/doom/i_scale.o games/doom/i_sound.o games/doom/i_system.o games/doom/i_timer.o games/doom/i_video.o \
            games/doom/icon.o games/doom/info.o games/doom/m_argv.o games/doom/m_bbox.o games/doom/m_cheat.o \
            games/doom/m_config.o games/doom/m_controls.o games/doom/m_fixed.o games/doom/m_menu.o games/doom/m_misc.o \
            games/doom/m_random.o games/doom/memio.o games/doom/mus2mid.o games/doom/p_ceilng.o games/doom/p_doors.o \
            games/doom/p_enemy.o games/doom/p_floor.o games/doom/p_inter.o games/doom/p_lights.o games/doom/p_map.o \
            games/doom/p_maputl.o games/doom/p_mobj.o games/doom/p_plats.o games/doom/p_pspr.o games/doom/p_saveg.o \
            games/doom/p_setup.o games/doom/p_sight.o games/doom/p_spec.o games/doom/p_switch.o games/doom/p_telept.o \
            games/doom/p_tick.o games/doom/p_user.o games/doom/r_bsp.o games/doom/r_data.o games/doom/r_draw.o \
            games/doom/r_main.o games/doom/r_plane.o games/doom/r_segs.o games/doom/r_sky.o games/doom/r_things.o \
            games/doom/s_sound.o games/doom/sha1.o games/doom/sounds.o games/doom/st_lib.o games/doom/st_stuff.o \
            games/doom/statdump.o games/doom/tables.o games/doom/v_video.o games/doom/w_checksum.o games/doom/w_file.o \
            games/doom/w_main.o games/doom/w_wad.o games/doom/wi_stuff.o games/doom/z_zone.o

# Default target
all: lemon.bin

# Linking
lemon.bin: $(OBJS) $(DOOM_OBJS) assets/doom1_wad.o
	$(CC) -m32 -nostdlib -no-pie -static -o lemon.bin $(OBJS) $(DOOM_OBJS) assets/doom1_wad.o -T linker.ld

assets/doom1_wad.o: assets/doom1.wad
	objcopy -I binary -O elf32-i386 -B i386 assets/doom1.wad assets/doom1_wad.o

games/doom/%.o: games/doom/%.c
	$(CC) $(CFLAGS) -Igames/doom -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare -Wno-missing-field-initializers -c $< -o $@

# Kernel objects
boot.o: boot.s
	$(CC) $(CFLAGS) -c boot.s -o boot.o

kernel.o: kernel.c include/bga.h include/desktop.h include/vga.h include/keyboard.h shell.h
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

vga.o: vga.c include/vga.h
	$(CC) $(CFLAGS) -c vga.c -o vga.o

keyboard.o: keyboard.c include/keyboard.h
	$(CC) $(CFLAGS) -c keyboard.c -o keyboard.o

shell.o: shell.c shell.h include/vga.h include/keyboard.h commands.h
	$(CC) $(CFLAGS) -c shell.c -o shell.o

commands.o: commands.c commands.h include/desktop.h
	$(CC) $(CFLAGS) -c commands.c -o commands.o

# CLI App objects
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

apps/nano.o: apps/nano.c include/nano.h include/vga.h include/keyboard.h include/filesystem.h
	$(CC) $(CFLAGS) -c apps/nano.c -o apps/nano.o

apps/vim.o: apps/vim.c include/vim.h include/vga.h include/keyboard.h include/filesystem.h
	$(CC) $(CFLAGS) -c apps/vim.c -o apps/vim.o

apps/less.o: apps/less.c include/less.h include/vga.h include/keyboard.h include/filesystem.h
	$(CC) $(CFLAGS) -c apps/less.c -o apps/less.o

apps/lua.o: apps/lua.c include/lua.h include/vga.h include/keyboard.h include/filesystem.h
	$(CC) $(CFLAGS) -c apps/lua.c -o apps/lua.o

apps/sqlite.o: apps/sqlite.c include/sqlite.h include/vga.h include/keyboard.h include/filesystem.h
	$(CC) $(CFLAGS) -c apps/sqlite.c -o apps/sqlite.o

apps/busybox.o: apps/busybox.c include/busybox.h include/filesystem.h
	$(CC) $(CFLAGS) -c apps/busybox.c -o apps/busybox.o

apps/tar_gzip.o: apps/tar_gzip.c include/tar_gzip.h include/filesystem.h
	$(CC) $(CFLAGS) -c apps/tar_gzip.c -o apps/tar_gzip.o

apps/ssh.o: apps/ssh.c include/ssh.h include/vga.h include/keyboard.h
	$(CC) $(CFLAGS) -c apps/ssh.c -o apps/ssh.o

apps/curl.o: apps/curl.c include/curl.h include/net.h include/filesystem.h
	$(CC) $(CFLAGS) -c apps/curl.c -o apps/curl.o

apps/speedtest.o: apps/speedtest.c include/speedtest.h include/gui_speedtest.h include/net.h
	$(CC) $(CFLAGS) -c apps/speedtest.c -o apps/speedtest.o

# Modern GUI Application objects
apps/gui_speedtest.o: apps/gui_speedtest.c include/gui_speedtest.h include/speedtest.h include/wm.h include/graphics.h include/font.h include/icons.h
	$(CC) $(CFLAGS) -c apps/gui_speedtest.c -o apps/gui_speedtest.o

apps/gui_firefox.o: apps/gui_firefox.c include/gui_firefox.h include/wm.h include/graphics.h include/font.h include/icons.h include/net.h include/filesystem.h include/desktop.h include/netsurf.h
	$(CC) $(CFLAGS) -c apps/gui_firefox.c -o apps/gui_firefox.o

apps/gui_terminal.o: apps/gui_terminal.c include/gui_terminal.h include/wm.h include/graphics.h include/font.h include/langrt.h include/apt.h
	$(CC) $(CFLAGS) -c apps/gui_terminal.c -o apps/gui_terminal.o

apps/gui_calc.o: apps/gui_calc.c include/gui_calc.h include/wm.h include/graphics.h include/font.h
	$(CC) $(CFLAGS) -c apps/gui_calc.c -o apps/gui_calc.o

apps/gui_converter.o: apps/gui_converter.c include/gui_converter.h include/wm.h include/graphics.h include/font.h
	$(CC) $(CFLAGS) -c apps/gui_converter.c -o apps/gui_converter.o

apps/gui_sysinfo.o: apps/gui_sysinfo.c include/gui_sysinfo.h include/wm.h include/graphics.h include/font.h
	$(CC) $(CFLAGS) -c apps/gui_sysinfo.c -o apps/gui_sysinfo.o

apps/gui_files.o: apps/gui_files.c include/gui_files.h include/wm.h include/graphics.h include/font.h include/filesystem.h
	$(CC) $(CFLAGS) -c apps/gui_files.c -o apps/gui_files.o

apps/gui_clock.o: apps/gui_clock.c include/gui_clock.h include/wm.h include/graphics.h include/font.h include/rtc.h
	$(CC) $(CFLAGS) -c apps/gui_clock.c -o apps/gui_clock.o

apps/gui_snake.o: apps/gui_snake.c include/gui_snake.h include/wm.h include/graphics.h include/font.h
	$(CC) $(CFLAGS) -c apps/gui_snake.c -o apps/gui_snake.o

apps/gui_breakout.o: apps/gui_breakout.c include/gui_breakout.h include/wm.h include/graphics.h include/font.h
	$(CC) $(CFLAGS) -c apps/gui_breakout.c -o apps/gui_breakout.o

apps/gui_pong.o: apps/gui_pong.c include/gui_pong.h include/wm.h include/graphics.h include/font.h
	$(CC) $(CFLAGS) -c apps/gui_pong.c -o apps/gui_pong.o

apps/gui_matrix.o: apps/gui_matrix.c include/gui_matrix.h include/wm.h include/graphics.h include/font.h
	$(CC) $(CFLAGS) -c apps/gui_matrix.c -o apps/gui_matrix.o

apps/gui_settings.o: apps/gui_settings.c include/gui_settings.h include/wm.h include/graphics.h include/font.h include/desktop.h
	$(CC) $(CFLAGS) -c apps/gui_settings.c -o apps/gui_settings.o

apps/gui_services.o: apps/gui_services.c include/gui_services.h include/wm.h include/graphics.h include/font.h include/icons.h include/service.h include/proc_mgr.h include/init.h
	$(CC) $(CFLAGS) -c apps/gui_services.c -o apps/gui_services.o

# Video & Graphics Driver objects
drivers/bga.o: drivers/bga.c include/bga.h
	$(CC) $(CFLAGS) -c drivers/bga.c -o drivers/bga.o

drivers/mouse.o: drivers/mouse.c include/mouse.h
	$(CC) $(CFLAGS) -c drivers/mouse.c -o drivers/mouse.o

drivers/font.o: drivers/font.c include/font.h include/icons.h
	$(CC) $(CFLAGS) -c drivers/font.c -o drivers/font.o

drivers/icons.o: drivers/icons.c include/icons.h include/icon_data.h include/graphics.h
	$(CC) $(CFLAGS) -c drivers/icons.c -o drivers/icons.o

drivers/icon_data.o: drivers/icon_data.c include/icon_data.h
	$(CC) $(CFLAGS) -c drivers/icon_data.c -o drivers/icon_data.o

drivers/graphics.o: drivers/graphics.c include/graphics.h include/bga.h include/font.h include/icons.h include/fast_mem.h
	$(CC) $(CFLAGS) -c drivers/graphics.c -o drivers/graphics.o

drivers/gui_engine.o: drivers/gui_engine.cpp include/gui_engine.h include/graphics.h include/font.h include/icons.h include/fast_mem.h
	g++ -nostdlib -fno-builtin -fno-stack-protector -fno-exceptions -fno-rtti -O2 -Wall -Wextra -m32 -Iinclude -c drivers/gui_engine.cpp -o drivers/gui_engine.o

drivers/wm.o: drivers/wm.c include/wm.h include/graphics.h include/font.h include/icons.h
	$(CC) $(CFLAGS) -c drivers/wm.c -o drivers/wm.o

drivers/desktop.o: drivers/desktop.c include/desktop.h include/graphics.h include/font.h include/icons.h include/mouse.h include/keyboard.h include/wm.h include/bga.h include/rtc.h include/alerts.h include/net.h
	$(CC) $(CFLAGS) -c drivers/desktop.c -o drivers/desktop.o

drivers/alerts.o: drivers/alerts.c include/alerts.h include/graphics.h include/font.h include/icons.h
	$(CC) $(CFLAGS) -c drivers/alerts.c -o drivers/alerts.o

drivers/netsurf.o: drivers/netsurf.c include/netsurf.h include/graphics.h include/font.h include/string.h
	$(CC) $(CFLAGS) -c drivers/netsurf.c -o drivers/netsurf.o

# PCI & Network Driver objects
drivers/pci.o: drivers/pci.c include/pci.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/pci.c -o drivers/pci.o

drivers/e1000.o: drivers/e1000.c include/e1000.h include/pci.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/e1000.c -o drivers/e1000.o

drivers/net.o: drivers/net.c include/net.h include/tls.h include/e1000.h include/filesystem.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/net.c -o drivers/net.o

drivers/tls.o: drivers/tls.c include/tls.h include/net.h include/entropy.h include/serial.h include/string.h
	$(CC) $(CFLAGS) -c drivers/tls.c -o drivers/tls.o

# Block Storage & VFS Driver objects
drivers/block.o: drivers/block.c include/block.h include/bcache.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/block.c -o drivers/block.o

drivers/bcache.o: drivers/bcache.c include/bcache.h include/block.h include/fast_mem.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/bcache.c -o drivers/bcache.o

drivers/ata.o: drivers/ata.c include/ata.h include/block.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/ata.c -o drivers/ata.o

drivers/ramdisk.o: drivers/ramdisk.c include/ramdisk.h include/block.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/ramdisk.c -o drivers/ramdisk.o

drivers/vfs.o: drivers/vfs.c include/vfs.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/vfs.c -o drivers/vfs.o

drivers/pipe.o: drivers/pipe.c include/pipe.h include/vfs.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/pipe.c -o drivers/pipe.o

drivers/devfs.o: drivers/devfs.c include/devfs.h include/vfs.h include/block.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/devfs.c -o drivers/devfs.o

drivers/fs_lemon.o: drivers/fs_lemon.c include/fs_lemon.h include/block.h include/vfs.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/fs_lemon.c -o drivers/fs_lemon.o

# System API & Userspace Loader objects
drivers/sys_asm.o: drivers/sys_asm.s
	$(CC) $(CFLAGS) -c drivers/sys_asm.s -o drivers/sys_asm.o

drivers/fast_mem.o: drivers/fast_mem.s include/fast_mem.h
	$(CC) $(CFLAGS) -c drivers/fast_mem.s -o drivers/fast_mem.o

drivers/slab.o: drivers/slab.cpp include/slab.h include/fast_mem.h
	g++ -nostdlib -fno-builtin -fno-stack-protector -fno-exceptions -fno-rtti -O2 -Wall -Wextra -m32 -Iinclude -c drivers/slab.cpp -o drivers/slab.o

drivers/gdt.o: drivers/gdt.c include/gdt.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/gdt.c -o drivers/gdt.o

drivers/idt.o: drivers/idt.c include/idt.h include/gdt.h include/syscall.h include/serial.h include/vga.h
	$(CC) $(CFLAGS) -c drivers/idt.c -o drivers/idt.o

drivers/syscall.o: drivers/syscall.c include/syscall.h include/vfs.h include/user_loader.h include/serial.h include/vga.h
	$(CC) $(CFLAGS) -c drivers/syscall.c -o drivers/syscall.o

drivers/user_loader.o: drivers/user_loader.c include/user_loader.h include/gdt.h include/vfs.h include/serial.h include/vga.h include/dynlink.h
	$(CC) $(CFLAGS) -c drivers/user_loader.c -o drivers/user_loader.o

drivers/dynlink.o: drivers/dynlink.c include/dynlink.h include/vfs.h include/serial.h include/vga.h include/string.h include/syscall.h
	$(CC) $(CFLAGS) -c drivers/dynlink.c -o drivers/dynlink.o

# Cryptography, Entropy, POSIX Time, Notifications, MMAN, Terminal Control
drivers/crypto.o: drivers/crypto.c include/crypto.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/crypto.c -o drivers/crypto.o

drivers/entropy.o: drivers/entropy.c include/entropy.h include/crypto.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/entropy.c -o drivers/entropy.o

drivers/posix_time.o: drivers/posix_time.c include/posix_time.h include/rtc.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/posix_time.c -o drivers/posix_time.o

drivers/audio.o: drivers/audio.c include/audio.h include/posix_time.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/audio.c -o drivers/audio.o

drivers/inotify.o: drivers/inotify.c include/inotify.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/inotify.c -o drivers/inotify.o

drivers/mman.o: drivers/mman.c include/mman.h include/vfs.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/mman.c -o drivers/mman.o

drivers/termios.o: drivers/termios.c include/termios.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/termios.c -o drivers/termios.o

# Process Manager, Service Daemons, and /proc Filesystem
drivers/proc_mgr.o: drivers/proc_mgr.c include/proc_mgr.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/proc_mgr.c -o drivers/proc_mgr.o

drivers/service.o: drivers/service.c include/service.h include/proc_mgr.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/service.c -o drivers/service.o

drivers/init.o: drivers/init.c include/init.h include/proc_mgr.h include/service.h include/vfs.h include/filesystem.h include/posix_time.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/init.c -o drivers/init.o

drivers/procfs.o: drivers/procfs.c include/procfs.h include/proc_mgr.h include/vfs.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/procfs.c -o drivers/procfs.o

# Developer Toolchain Suite (Compiler, Make, Git, APT)
drivers/compiler.o: drivers/compiler.c include/compiler.h include/vfs.h include/user_loader.h include/filesystem.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/compiler.c -o drivers/compiler.o

drivers/langrt.o: drivers/langrt.c include/langrt.h include/compiler.h include/vfs.h include/filesystem.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/langrt.c -o drivers/langrt.o

drivers/make.o: drivers/make.c include/make.h include/compiler.h include/vfs.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/make.c -o drivers/make.o

drivers/git.o: drivers/git.c include/git.h include/crypto.h include/vfs.h include/posix_time.h include/filesystem.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/git.c -o drivers/git.o

drivers/apt.o: drivers/apt.c include/apt.h include/crypto.h include/vfs.h include/filesystem.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/apt.c -o drivers/apt.o

drivers/auth.o: drivers/auth.c include/auth.h include/filesystem.h include/vfs.h include/string.h include/serial.h include/vga.h
	$(CC) $(CFLAGS) -c drivers/auth.c -o drivers/auth.o

drivers/pmm.o: drivers/pmm.c include/pmm.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/pmm.c -o drivers/pmm.o

drivers/vmm.o: drivers/vmm.c include/vmm.h include/pmm.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/vmm.c -o drivers/vmm.o

drivers/kmem.o: drivers/kmem.c include/kmem.h include/pmm.h include/vmm.h include/fast_mem.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/kmem.c -o drivers/kmem.o

drivers/sync.o: drivers/sync.c include/sync.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/sync.c -o drivers/sync.o

drivers/signal.o: drivers/signal.c include/signal.h include/proc_mgr.h include/sync.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/signal.c -o drivers/signal.o

drivers/shm.o: drivers/shm.c include/shm.h include/pmm.h include/sync.h include/proc_mgr.h include/serial.h
	$(CC) $(CFLAGS) -c drivers/shm.c -o drivers/shm.o

# Libc Targets
libc/crt0.o: libc/src/crt0.s
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/crt0.s -o libc/crt0.o

libc/libc.a: libc/src/*.c libc/src/setjmp.s
	mkdir -p libc/build
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/setjmp.s -o libc/build/setjmp.o
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/assert.c -o libc/build/assert.o
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/ctype.c -o libc/build/ctype.o
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/dirent.c -o libc/build/dirent.o
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/math.c -o libc/build/math.o
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/mman.c -o libc/build/mman.o
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/signal.c -o libc/build/signal.o
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/stat.c -o libc/build/stat.o
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/stdio.c -o libc/build/stdio.o
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/stdlib.c -o libc/build/stdlib.o
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/string.c -o libc/build/string.o
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/syscalls.c -o libc/build/syscalls.o
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/termios.c -o libc/build/termios.o
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/time.c -o libc/build/time.o
	$(CC) -m32 -c -nostdlib -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Ilibc/include libc/src/unistd.c -o libc/build/unistd.o
	ar rcs libc/libc.a libc/build/*.o

libc/libc.so user/libc.so: libc/src/*.c libc/src/setjmp.s
	$(CC) -m32 -shared -fPIC -nostdlib -fno-builtin -fno-stack-protector -Wl,--hash-style=both -Wl,-soname,libc.so -Ilibc/include -o libc/libc.so libc/src/*.c libc/src/setjmp.s
	cp libc/libc.so user/libc.so

user/dynhello.elf: user/dynhello.c user/libc.so
	$(CC) -m32 -nostdlib -fno-builtin -fno-stack-protector -fPIC -pie -Wl,--hash-style=both user/dynhello.c user/libc.so -o user/dynhello.elf -Wl,-rpath,/lib

user/dynposix.elf: user/dynposix.c user/libc.so libc/crt0.o
	$(CC) -m32 -nostdlib -fno-builtin -fno-stack-protector -fPIC -pie -Wl,--hash-style=both -Ilibc/include libc/crt0.o user/dynposix.c user/libc.so -o user/dynposix.elf -Wl,-rpath,/lib

USER_PROGS = user/hello.elf user/echo.elf user/sysinfo.elf user/cat.elf user/usertest.elf user/posixtest.elf user/libctest.elf user/libc.so user/dynhello.elf user/dynposix.elf

include/user_elf_bins.h: $(USER_PROGS)
	python3 scripts/gen_elf_bins.py

user/%.elf: user/%.c libc/crt0.o libc/libc.a libc/user.ld
	$(CC) -m32 -nostdlib -fno-builtin -fno-stack-protector -fno-pic -fno-pie -mno-sse -mno-sse2 -O2 -Ilibc/include -no-pie -static -T libc/user.ld libc/crt0.o $< libc/libc.a -o $@

apps/user_programs.o: apps/user_programs.c include/user_programs.h include/user_elf_bins.h include/user_loader.h include/vfs.h include/serial.h
	$(CC) $(CFLAGS) -c apps/user_programs.c -o apps/user_programs.o

# Legacy Driver objects
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
	
drivers/stdlib.o: drivers/stdlib.c include/stdlib.h include/kmem.h include/string.h
	$(CC) $(CFLAGS) -c drivers/stdlib.c -o drivers/stdlib.o

drivers/stdio.o: drivers/stdio.c include/stdio.h include/vga.h include/string.h
	$(CC) $(CFLAGS) -c drivers/stdio.c -o drivers/stdio.o

drivers/string.o: drivers/string.c include/string.h
	$(CC) $(CFLAGS) -c drivers/string.c -o drivers/string.o

drivers/serial.o: drivers/serial.c include/serial.h
	$(CC) $(CFLAGS) -c drivers/serial.c -o drivers/serial.o
	
# Game objects
games/snake.o: games/snake.c include/snake.h include/text_graphics.h include/vga.h include/keyboard.h
	$(CC) $(CFLAGS) -c games/snake.c -o games/snake.o
	
games/breakout.o: games/breakout.c include/breakout.h include/text_graphics.h include/vga.h include/keyboard.h
	$(CC) $(CFLAGS) -c games/breakout.c -o games/breakout.o
	
games/pong.o: games/pong.c include/pong.h include/vga.h include/keyboard.h include/vga_utils.h
	$(CC) $(CFLAGS) -c games/pong.c -o games/pong.o

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

.PHONY: all clean run iso debug release
