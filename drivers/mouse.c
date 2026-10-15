#include "mouse.h"
#include "serial.h"
#include <stdint.h>

#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_COMMAND_PORT 0x64

static int mouse_x = 0;
static int mouse_y = 0;
static int mouse_max_x = 1024;
static int mouse_max_y = 768;

static uint8_t mouse_buttons = 0;
static uint8_t mouse_prev_buttons = 0;
static int mouse_left_clicked_latched = 0;
static int mouse_right_clicked_latched = 0;
static int mouse_left_released_latched = 0;
static int mouse_right_released_latched = 0;

static uint8_t mouse_has_wheel = 0;
static uint8_t mouse_packet_size = 3;
static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[4] = {0, 0, 0, 0};
static int mouse_enabled = 0;
static int mouse_dx_accum = 0;
static int mouse_dy_accum = 0;
static int mouse_scroll_accum = 0;

int mouse_is_enabled(void) {
    return mouse_enabled;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void mouse_wait_write(void) {
    int timeout = 100000;
    while ((inb(PS2_STATUS_PORT) & 0x02) && --timeout) {
        asm volatile("pause");
    }
}

static void mouse_wait_read(void) {
    int timeout = 100000;
    while (!(inb(PS2_STATUS_PORT) & 0x01) && --timeout) {
        asm volatile("pause");
    }
}

static void mouse_write(uint8_t val) {
    mouse_wait_write();
    outb(PS2_COMMAND_PORT, 0xD4);
    mouse_wait_write();
    outb(PS2_DATA_PORT, val);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(PS2_DATA_PORT);
}

void mouse_init(int max_x, int max_y) {
    mouse_max_x = max_x;
    mouse_max_y = max_y;
    mouse_x = max_x / 2;
    mouse_y = max_y / 2;
    mouse_buttons = 0;
    mouse_prev_buttons = 0;
    mouse_left_clicked_latched = 0;
    mouse_right_clicked_latched = 0;
    mouse_left_released_latched = 0;
    mouse_right_released_latched = 0;
    mouse_cycle = 0;

    // Drain any leftover data in PS/2 buffer
    int drain = 100;
    while ((inb(PS2_STATUS_PORT) & 0x01) && --drain) {
        inb(PS2_DATA_PORT);
    }

    // Enable auxiliary mouse device
    mouse_wait_write();
    outb(PS2_COMMAND_PORT, 0xA8);

    // Read controller configuration byte
    mouse_wait_write();
    outb(PS2_COMMAND_PORT, 0x20);
    mouse_wait_read();
    uint8_t status = inb(PS2_DATA_PORT);

    // Enable mouse IRQ12 and enable device clock
    status |= 0x02;
    status &= ~0x20;

    // Write back controller configuration byte
    mouse_wait_write();
    outb(PS2_COMMAND_PORT, 0x60);
    mouse_wait_write();
    outb(PS2_DATA_PORT, status);

    // Tell mouse to use default settings
    mouse_write(0xF6);
    mouse_read(); // ACK (0xFA)

    // IntelliMouse Knock Sequence: sample rates 200 -> 100 -> 80
    mouse_write(0xF3);
    mouse_read();
    mouse_write(200);
    mouse_read();

    mouse_write(0xF3);
    mouse_read();
    mouse_write(100);
    mouse_read();

    mouse_write(0xF3);
    mouse_read();
    mouse_write(80);
    mouse_read();

    // Query Device ID
    mouse_write(0xF2);
    uint8_t ack = mouse_read();
    uint8_t mouse_id = mouse_read();
    serial_print("[MOUSE] 0xF2 response: ack=");
    serial_print_hex(ack);
    serial_print(" id=");
    serial_print_hex(mouse_id);
    serial_println("");
    if (mouse_id == 3 || mouse_id == 4) {
        mouse_has_wheel = 1;
        mouse_packet_size = 4;
        serial_println("[MOUSE] IntelliMouse wheel support ENABLED (4-byte packets)");
    } else {
        mouse_has_wheel = 0;
        mouse_packet_size = 3;
        serial_println("[MOUSE] Standard PS/2 mouse (3-byte packets)");
    }

    // Enable packet streaming
    mouse_write(0xF4);
    mouse_read(); // ACK (0xFA)

    mouse_enabled = 1;
}

void mouse_poll(void) {
    if (!mouse_enabled) return;

    mouse_prev_buttons = mouse_buttons;

    int max_bytes = 24;
    while (--max_bytes) {
        uint8_t status = inb(PS2_STATUS_PORT);
        if (!(status & 0x01)) {
            break;
        }

        // Check if data is from mouse (bit 5 in status port)
        if (!(status & 0x20)) {
            // Data is for keyboard: leave in port 0x60 for poll_keyboard()!
            break;
        }

        uint8_t data = inb(PS2_DATA_PORT);

        switch (mouse_cycle) {
            case 0:
                // Packet byte 0: Bit 3 must be 1 for valid mouse packet
                if (data & 0x08) {
                    mouse_packet[0] = data;
                    mouse_cycle = 1;
                }
                break;
            case 1:
                mouse_packet[1] = data;
                mouse_cycle = 2;
                break;
            case 2:
                mouse_packet[2] = data;
                if (mouse_packet_size == 4) {
                    mouse_cycle = 3;
                    break;
                }
                mouse_cycle = 0;
                goto process_packet;
            case 3:
                mouse_packet[3] = data;
                mouse_cycle = 0;
                goto process_packet;

            process_packet:
                // Only process packet if no overflow bits are set (bits 6, 7)
                if (!(mouse_packet[0] & 0xC0)) {
                    int dx = (int8_t)mouse_packet[1];
                    int dy = (int8_t)mouse_packet[2];

                    mouse_x += dx;
                    // PS/2 mouse Y axis is inverted (positive is up, screen down is positive)
                    mouse_y -= dy;

                    mouse_dx_accum += dx;
                    mouse_dy_accum += dy;

                    if (mouse_packet_size == 4) {
                        int dz = -(int8_t)mouse_packet[3];
                        if (dz != 0) {
                            mouse_scroll_accum += dz;
                        }
                    }

                    if (mouse_x < 0) mouse_x = 0;
                    if (mouse_x >= mouse_max_x) mouse_x = mouse_max_x - 1;
                    if (mouse_y < 0) mouse_y = 0;
                    if (mouse_y >= mouse_max_y) mouse_y = mouse_max_y - 1;

                    uint8_t new_buttons = mouse_packet[0] & 0x07;
                    if ((new_buttons & MOUSE_BTN_LEFT) && !(mouse_buttons & MOUSE_BTN_LEFT)) {
                        mouse_left_clicked_latched = 1;
                    }
                    if (!(new_buttons & MOUSE_BTN_LEFT) && (mouse_buttons & MOUSE_BTN_LEFT)) {
                        mouse_left_released_latched = 1;
                    }
                    if ((new_buttons & MOUSE_BTN_RIGHT) && !(mouse_buttons & MOUSE_BTN_RIGHT)) {
                        mouse_right_clicked_latched = 1;
                    }
                    if (!(new_buttons & MOUSE_BTN_RIGHT) && (mouse_buttons & MOUSE_BTN_RIGHT)) {
                        mouse_right_released_latched = 1;
                    }
                    mouse_buttons = new_buttons;
                }
                break;
        }
    }
}

int mouse_get_x(void) {
    return mouse_x;
}

int mouse_get_y(void) {
    return mouse_y;
}

int mouse_get_buttons(void) {
    return mouse_buttons;
}

int mouse_get_delta(int* dx, int* dy) {
    if (dx) *dx = mouse_dx_accum;
    if (dy) *dy = mouse_dy_accum;
    int moved = (mouse_dx_accum != 0 || mouse_dy_accum != 0);
    mouse_dx_accum = 0;
    mouse_dy_accum = 0;
    return moved;
}

int mouse_is_left_pressed(void) {
    return (mouse_buttons & MOUSE_BTN_LEFT) != 0;
}

int mouse_is_right_pressed(void) {
    return (mouse_buttons & MOUSE_BTN_RIGHT) != 0;
}

int mouse_is_middle_pressed(void) {
    return (mouse_buttons & MOUSE_BTN_MIDDLE) != 0;
}

int mouse_is_left_clicked(void) {
    if (mouse_left_clicked_latched) {
        mouse_left_clicked_latched = 0;
        return 1;
    }
    return ((mouse_buttons & MOUSE_BTN_LEFT) && !(mouse_prev_buttons & MOUSE_BTN_LEFT));
}

int mouse_is_right_clicked(void) {
    if (mouse_right_clicked_latched) {
        mouse_right_clicked_latched = 0;
        return 1;
    }
    return ((mouse_buttons & MOUSE_BTN_RIGHT) && !(mouse_prev_buttons & MOUSE_BTN_RIGHT));
}

int mouse_is_left_released(void) {
    if (mouse_left_released_latched) {
        mouse_left_released_latched = 0;
        return 1;
    }
    return (!(mouse_buttons & MOUSE_BTN_LEFT) && (mouse_prev_buttons & MOUSE_BTN_LEFT));
}

int mouse_get_scroll_delta(void) {
    int s = mouse_scroll_accum;
    mouse_scroll_accum = 0;
    return s;
}

int mouse_has_scroll_wheel(void) {
    return mouse_has_wheel;
}

void mouse_set_bounds(int max_x, int max_y) {
    mouse_max_x = max_x;
    mouse_max_y = max_y;
    if (mouse_x >= max_x) mouse_x = max_x - 1;
    if (mouse_y >= max_y) mouse_y = max_y - 1;
}

void mouse_set_position(int x, int y) {
    mouse_x = x;
    mouse_y = y;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= mouse_max_x) mouse_x = mouse_max_x - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= mouse_max_y) mouse_y = mouse_max_y - 1;
}
