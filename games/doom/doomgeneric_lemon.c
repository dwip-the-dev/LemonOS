// LemonOS Doom Platform Layer
// Implements doomgeneric interfaces for LemonOS (Video, Timing, Input, Audio)

#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bga.h"
#include "keyboard.h"
#include "mouse.h"
#include "posix_time.h"
#include "audio.h"
#include "vga.h"
#include "serial.h"

#define SCREEN_W 1024
#define SCREEN_H 768

static uint32_t* s_FrameBuffer = NULL;
static uint32_t s_OffsetX = (SCREEN_W - DOOMGENERIC_RESX) / 2; // (1024 - 640) / 2 = 192
static uint32_t s_OffsetY = (SCREEN_H - DOOMGENERIC_RESY) / 2; // (768 - 400) / 2 = 184

void DG_Init(void) {
    serial_print("[DOOM] Initializing LemonOS Doom platform layer...\n");

    // Initialize/switch BGA display mode to 1024x768 32bpp
    if (bga_is_available()) {
        bga_set_video_mode(SCREEN_W, SCREEN_H, 32, 1, 1);
    }
    s_FrameBuffer = (uint32_t*)bga_get_framebuffer();

    // Clear screen to black
    if (s_FrameBuffer) {
        memset(s_FrameBuffer, 0, SCREEN_W * SCREEN_H * sizeof(uint32_t));
    }

    // Clear input event queues and ensure mouse driver is active for Doom
    keyboard_clear_events();
    mouse_init(SCREEN_W, SCREEN_H);

    // Initialize timer calibration (RDTSC + PIT Channel 2)
    posix_time_init();

    // Initialize Audio subsystem (Sound Blaster 16 DSP at 0x220 + PC Speaker)
    audio_init();
    audio_open(11025, AUDIO_FORMAT_U8, 1);

    serial_print("[DOOM] Platform layer ready: 1024x768x32 BGA FB, SB16 Audio, Monotonic Timer\n");
}

void DG_DrawFrame(void) {
    if (!s_FrameBuffer || !DG_ScreenBuffer) return;

    // Blit centered 640x400 frame buffer onto 1024x768 screen
    for (int y = 0; y < DOOMGENERIC_RESY; y++) {
        uint32_t* dst = s_FrameBuffer + (s_OffsetY + y) * SCREEN_W + s_OffsetX;
        const uint32_t* src = (const uint32_t*)DG_ScreenBuffer + y * DOOMGENERIC_RESX;
        memcpy(dst, src, DOOMGENERIC_RESX * sizeof(uint32_t));
    }
}

void DG_SleepMs(uint32_t ms) {
    timer_sleep_ms(ms);
}

static volatile int doom_running = 1;

void doom_exit(void) {
    doom_running = 0;
}

uint32_t DG_GetTicksMs(void) {
    return (uint32_t)timer_get_ticks_ms();
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
    if (!doom_running) return 0;
    int ret = keyboard_poll_event(pressed, doomKey);
    if (ret && pressed && *pressed) {
        // Alt + F4 (doom key for F4 is 0x80+0x3E, scancode is 0x3E, special is 0x04)
        if (is_alt_pressed() && (*doomKey == KEY_F4 || *doomKey == 0x04 || *doomKey == 0x3E)) {
            serial_print("[DOOM] Alt+F4 pressed! Terminating game...\n");
            doom_exit();
            return 0;
        }
        // Alt + Q shortcut as alternative quit
        if (is_alt_pressed() && (*doomKey == 'q' || *doomKey == 'Q')) {
            serial_print("[DOOM] Alt+Q pressed! Terminating game...\n");
            doom_exit();
            return 0;
        }
    }
    return ret;
}

void DG_SetWindowTitle(const char * title) {
    (void)title;
}

// Global entry point invoked by LemonOS shell/GUI
void doom_main(int argc, char** argv) {
    serial_print("\n=========================================\n");
    serial_print("   LEMONOS DOOM ENGINE INITIALIZING\n");
    serial_print("=========================================\n");

    doom_running = 1;
    doomgeneric_Create(argc, argv);

    serial_print("[DOOM] Entering main game tick loop...\n");
    while (doom_running) {
        doomgeneric_Tick();
    }

    serial_print("[DOOM] Doom game loop terminated cleanly. Shutting down audio & clearing events...\n");
    audio_close();
    keyboard_clear_events();
}

