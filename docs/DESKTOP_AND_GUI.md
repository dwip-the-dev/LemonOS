# LemonOS Desktop Environment & Graphical Subsystem

**Author:** Dwip Biswas ([@dwip-the-dev](https://github.com/dwip-the-dev))  
**Resolution:** 1024 x 768 @ 32-bit Truecolor (ARGB8888)  
**Display Backend:** Bochs / QEMU VBE Graphics Adapter (BGA)  

---

## 1. Graphics Pipeline & Hardware Acceleration

LemonOS utilizes the Bochs Graphics Adapter (BGA) I/O port interface (ports `0x01CE` / `0x01CF`):

- **Linear Framebuffer (LFB):** Discovered via PCI configuration space and mapped to high virtual memory.
- **Double Buffering:** All draw calls render to an offscreen backbuffer in system RAM before a fast optimized 32-bit blit copies dirty regions to video RAM, eliminating screen tearing.
- **Alpha Blending:** True 8-bit alpha compositing for translucent window shadows, rounded corner smoothing, dock magnification, and cursor rendering.

---

## 2. Window Manager (`drivers/wm.c`, `include/wm.h`)

The LemonOS Window Manager provides a modern desktop user experience:

- **Z-Ordering & Occlusion:** Windows maintain hierarchical depth ordering; clicking any window raises it to the top.
- **Window Decorations:**
  - Modern titlebars with gradient headers.
  - Interactive window control buttons (Close, Minimize, Maximize).
  - Resizable borders with drag-handle hitboxes.
- **Move & Drag:** Dragging window titlebars smoothly translates window coordinates across the desktop.
- **Taskbar & Dock:**
  - Dynamic dock with app launcher shortcuts and active running indicators.
  - System status bar with real-time RTC clock, CPU/Memory load gauges, and volume controls.
  - Application switcher (Alt+Tab / Overview mode) displaying live thumbnail previews.

---

## 3. Graphical Applications

| Application | File | Description |
|-------------|------|-------------|
| **NetSurf / Firefox** | `apps/gui_firefox.c`, `drivers/netsurf.c` | Graphical browser with HTML5 DOM tree parsing, CSS cascading, and URL bar. |
| **Terminal Emulator** | `apps/gui_terminal.c` | Full graphical terminal connected to POSIX PTY/devfs with ANSI color support. |
| **File Manager** | `apps/gui_files.c` | Visual file explorer with directory navigation, icons, and file creation. |
| **System Monitor** | `apps/gui_sysinfo.c` | Live graphs of CPU utilization, RAM usage, and active process lists. |
| **Calculator** | `apps/gui_calc.c` | Graphical calculator with button click events and decimal operations. |
| **Unit Converter** | `apps/gui_converter.c` | Interactive currency, distance, temperature, and speed conversions. |
| **Settings & Control** | `apps/gui_settings.c` | Desktop theme, resolution, timezone, and network settings. |
| **Matrix Screensaver** | `apps/gui_matrix.c` | Real-time digital rain animation with phosphorescent green trails. |
| **Audio Synthesizer** | `drivers/audio.c` | Interactive sound synthesizer for PC Speaker and Sound Blaster. |

---

## 4. Classic DOOM Port (`games/doom/`)

LemonOS includes a complete, baremetal port of id Software's classic **DOOM (Doom 1 / Shareware E1M1)**:

- **Engine:** `doomgeneric` abstraction layer interfacing directly with LemonOS graphics and timer drivers.
- **WAD Embedding:** Packed `doom1.wad` embedded into kernel ELF image via `objcopy`.
- **Display Modes:** Runs both in standalone fullscreen mode and inside an interactive window on the graphical desktop.
- **Controls:** PS/2 keyboard navigation (WASD / Arrow Keys, Ctrl to fire, Space to open doors) and mouse aiming.
