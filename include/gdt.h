#ifndef GDT_H
#define GDT_H

#include <stdint.h>
#include <stddef.h>

// Segment Selectors
#define KERNEL_CS_SEL   0x08
#define KERNEL_DS_SEL   0x10
#define USER_CS_SEL     (0x18 | 3)  // Ring 3 Code Segment (0x1B)
#define USER_DS_SEL     (0x20 | 3)  // Ring 3 Data Segment (0x23)
#define TSS_SEL         0x28

// GDT Entry structure
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));
typedef struct gdt_entry gdt_entry_t;

// GDT Pointer structure for LGDT
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));
typedef struct gdt_ptr gdt_ptr_t;

// x86 Task State Segment (TSS)
struct tss_entry {
    uint32_t prev_tss;   // Previous TSS (unused with software task switching)
    uint32_t esp0;       // Kernel stack pointer
    uint32_t ss0;        // Kernel stack segment (0x10)
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));
typedef struct tss_entry tss_entry_t;

// GDT API
void gdt_init(void);
void gdt_set_kernel_stack(uint32_t stack_top);
uint32_t gdt_get_kernel_stack(void);

#endif
