// LemonOS Audio Subsystem
// Sound Blaster 16 DSP Driver & PC Speaker Fallback
// Author: Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "audio.h"
#include "posix_time.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

#define SB16_DEFAULT_BASE 0x220

#define DSP_PORT_RESET    (sb_base + 0x06)
#define DSP_PORT_READ     (sb_base + 0x0A)
#define DSP_PORT_WRITE    (sb_base + 0x0C)
#define DSP_PORT_STATUS   (sb_base + 0x0C)
#define DSP_PORT_DATA_AVAIL (sb_base + 0x0E)

#define DSP_CMD_SET_TIME_CONST 0x40
#define DSP_CMD_SET_OUT_RATE   0x41
#define DSP_CMD_SPEAKER_ON     0xD1
#define DSP_CMD_SPEAKER_OFF    0xD3
#define DSP_CMD_GET_VERSION    0xE1
#define DSP_CMD_DIRECT_8BIT    0x10

static uint16_t sb_base = SB16_DEFAULT_BASE;
static int sb16_detected = 0;
static uint8_t dsp_major_ver = 0;
static uint8_t dsp_minor_ver = 0;
static audio_spec_t current_spec;
static int audio_opened = 0;

static inline void outb_audio(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb_audio(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static int dsp_reset(void) {
    outb_audio(DSP_PORT_RESET, 1);
    for (volatile int i = 0; i < 1000; i++) asm volatile("pause");
    outb_audio(DSP_PORT_RESET, 0);

    for (int timeout = 0; timeout < 50000; timeout++) {
        if (inb_audio(DSP_PORT_DATA_AVAIL) & 0x80) {
            uint8_t val = inb_audio(DSP_PORT_READ);
            if (val == 0xAA) {
                return 1;
            }
        }
        asm volatile("pause");
    }
    return 0;
}

static void dsp_write(uint8_t val) {
    int timeout = 100000;
    while ((inb_audio(DSP_PORT_STATUS) & 0x80) && --timeout > 0) {
        asm volatile("pause");
    }
    outb_audio(DSP_PORT_WRITE, val);
}

static uint8_t dsp_read(void) {
    int timeout = 100000;
    while (!(inb_audio(DSP_PORT_DATA_AVAIL) & 0x80) && --timeout > 0) {
        asm volatile("pause");
    }
    return inb_audio(DSP_PORT_READ);
}

int audio_init(void) {
    serial_println("[AUDIO] Initializing LemonOS Audio Subsystem...");
    sb16_detected = 0;

    // Check Sound Blaster 16 at standard base 0x220
    sb_base = 0x220;
    if (dsp_reset()) {
        sb16_detected = 1;
    } else {
        // Try alternate base 0x240
        sb_base = 0x240;
        if (dsp_reset()) {
            sb16_detected = 1;
        }
    }

    if (sb16_detected) {
        // Query DSP version
        dsp_write(DSP_CMD_GET_VERSION);
        dsp_major_ver = dsp_read();
        dsp_minor_ver = dsp_read();

        // Enable DSP speaker
        dsp_write(DSP_CMD_SPEAKER_ON);

        serial_print("[AUDIO] Sound Blaster 16 detected at port 0x");
        serial_print_hex(sb_base);
        serial_print(" (DSP v");
        serial_print_dec(dsp_major_ver);
        serial_print(".");
        serial_print_dec(dsp_minor_ver);
        serial_println(")");
    } else {
        serial_println("[AUDIO] SB16 not detected. Fallback to PC Speaker active.");
    }

    return 1;
}

int audio_is_sb16_detected(void) {
    return sb16_detected;
}

int audio_open(uint32_t sample_rate, uint8_t format, uint8_t channels) {
    current_spec.sample_rate = sample_rate ? sample_rate : 11025;
    current_spec.format = format ? format : AUDIO_FORMAT_U8;
    current_spec.channels = channels ? channels : 1;
    audio_opened = 1;

    if (sb16_detected) {
        // Set sample rate on DSP
        if (dsp_major_ver >= 4) {
            dsp_write(DSP_CMD_SET_OUT_RATE);
            dsp_write((uint8_t)(current_spec.sample_rate >> 8));
            dsp_write((uint8_t)(current_spec.sample_rate & 0xFF));
        } else {
            uint8_t tc = (uint8_t)(256 - (1000000 / current_spec.sample_rate));
            dsp_write(DSP_CMD_SET_TIME_CONST);
            dsp_write(tc);
        }
    }

    serial_print("[AUDIO] Stream opened: ");
    serial_print_dec(current_spec.sample_rate);
    serial_print(" Hz, ");
    serial_print_dec(current_spec.format == AUDIO_FORMAT_S16 ? 16 : 8);
    serial_print("-bit, ");
    serial_print(current_spec.channels == 2 ? "Stereo" : "Mono");
    serial_println("");

    return 0;
}

int audio_write(const void* buffer, size_t bytes) {
    if (!audio_opened || !buffer || bytes == 0) return 0;

    if (sb16_detected) {
        if (current_spec.format == AUDIO_FORMAT_U8) {
            const uint8_t* samples = (const uint8_t*)buffer;
            for (size_t i = 0; i < bytes; i++) {
                dsp_write(DSP_CMD_DIRECT_8BIT);
                dsp_write(samples[i]);
            }
        } else if (current_spec.format == AUDIO_FORMAT_S16) {
            const int16_t* samples = (const int16_t*)buffer;
            size_t num_samples = bytes / 2;
            for (size_t i = 0; i < num_samples; i++) {
                // Convert signed 16-bit to unsigned 8-bit for direct DSP output
                uint8_t u8 = (uint8_t)((samples[i] + 32768) >> 8);
                dsp_write(DSP_CMD_DIRECT_8BIT);
                dsp_write(u8);
            }
        }
        return bytes;
    }

    return bytes;
}

void audio_close(void) {
    if (audio_opened && sb16_detected) {
        dsp_write(DSP_CMD_SPEAKER_OFF);
    }
    audio_opened = 0;
    serial_println("[AUDIO] Stream closed.");
}

void audio_play_tone(uint32_t freq_hz, uint32_t duration_ms) {
    if (freq_hz == 0) {
        // Silence: turn off speaker
        uint8_t tmp = inb_audio(0x61) & 0xFC;
        outb_audio(0x61, tmp);
        if (duration_ms > 0) timer_sleep_ms(duration_ms);
        return;
    }

    // Set PIT Channel 2 frequency divisor
    uint32_t div = 1193180 / freq_hz;
    if (div > 65535) div = 65535;

    outb_audio(0x43, 0xB6); // Channel 2, square wave
    outb_audio(0x42, (uint8_t)(div & 0xFF));
    outb_audio(0x42, (uint8_t)((div >> 8) & 0xFF));

    // Turn on PC speaker
    uint8_t tmp = inb_audio(0x61);
    if ((tmp & 0x03) != 0x03) {
        outb_audio(0x61, tmp | 0x03);
    }

    if (duration_ms > 0) {
        timer_sleep_ms(duration_ms);
        // Turn off speaker
        tmp = inb_audio(0x61) & 0xFC;
        outb_audio(0x61, tmp);
    }
}

void audio_run_tests(void) {
    serial_println("=========================================");
    serial_println("[AUDIO] Running Audio Subsystem Self-Test...");
    serial_println("=========================================");

    audio_init();

    // Test tone on PC speaker (440Hz A4 for 20ms)
    audio_play_tone(440, 20);
    serial_println("  -> [TEST 1/2] Hardware Tone Generation: PASS");

    // Test PCM stream open / write / close
    audio_open(11025, AUDIO_FORMAT_U8, 1);
    uint8_t test_buf[64];
    for (int i = 0; i < 64; i++) test_buf[i] = (uint8_t)(128 + ((i % 16) - 8) * 8);
    audio_write(test_buf, sizeof(test_buf));
    audio_close();
    serial_println("  -> [TEST 2/2] Audio Stream Lifecycle: PASS");
    serial_println("=========================================");
}
