#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include <stddef.h>

#define IDT_NUM_ENTRIES 256

// Gate flags
#define IDT_FLAG_PRESENT    0x80
#define IDT_FLAG_RING0      0x00
#define IDT_FLAG_RING3      0x60
#define IDT_FLAG_INT32      0x0E
#define IDT_FLAG_TRAP32     0x0F

#define IDT_GATE_KERNEL_INT (IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INT32) // 0x8E
#define IDT_GATE_USER_INT   (IDT_FLAG_PRESENT | IDT_FLAG_RING3 | IDT_FLAG_INT32) // 0xEE

// IDT Entry
struct idt_entry {
    uint16_t base_low;   // Lower 16 bits of handler address
    uint16_t sel;        // Kernel segment selector (0x08)
    uint8_t  always0;    // Always 0
    uint8_t  flags;      // Type and attributes
    uint16_t base_high;  // Upper 16 bits of handler address
} __attribute__((packed));
typedef struct idt_entry idt_entry_t;

// IDT Pointer for LIDT
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));
typedef struct idt_ptr idt_ptr_t;

void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

#endif
