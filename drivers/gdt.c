#include "gdt.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

#define GDT_NUM_ENTRIES 6

static gdt_entry_t gdt_entries[GDT_NUM_ENTRIES];
static gdt_ptr_t   gdt_p;
static tss_entry_t tss;

// Static kernel stack buffer for default Ring 0 interrupts
static uint8_t default_kernel_stack[16384] __attribute__((aligned(16)));

extern void gdt_flush_asm(uint32_t gdt_ptr_addr);
extern void tss_flush_asm(void);

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;
    gdt_entries[num].granularity |= (gran & 0xF0);
    gdt_entries[num].access      = access;
}

static void write_tss(int num, uint16_t ss0, uint32_t esp0) {
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(tss_entry_t) - 1;

    gdt_set_gate(num, base, limit, 0x89, 0x00); // 0x89: Present, Ring 0, Type 9 Available 32-bit TSS

    uint8_t* p = (uint8_t*)&tss;
    for (size_t i = 0; i < sizeof(tss_entry_t); i++) {
        p[i] = 0;
    }

    tss.ss0  = ss0;
    tss.esp0 = esp0;

    // Default segment registers
    tss.cs = KERNEL_CS_SEL;
    tss.ss = KERNEL_DS_SEL;
    tss.ds = KERNEL_DS_SEL;
    tss.es = KERNEL_DS_SEL;
    tss.fs = KERNEL_DS_SEL;
    tss.gs = KERNEL_DS_SEL;
    tss.iomap_base = sizeof(tss_entry_t);
}

void gdt_init(void) {
    serial_println("[GDT] Initializing Global Descriptor Table & TSS...");

    gdt_p.limit = (sizeof(gdt_entry_t) * GDT_NUM_ENTRIES) - 1;
    gdt_p.base  = (uint32_t)&gdt_entries;

    // 0: Null descriptor
    gdt_set_gate(0, 0, 0, 0, 0);

    // 1: Kernel Code Segment (0x08, Base=0, Limit=4GB, Ring 0, Read/Execute)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // 2: Kernel Data Segment (0x10, Base=0, Limit=4GB, Ring 0, Read/Write)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // 3: User Code Segment (0x18 | 3 = 0x1B, Base=0, Limit=4GB, Ring 3, Read/Execute)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    // 4: User Data Segment (0x20 | 3 = 0x23, Base=0, Limit=4GB, Ring 3, Read/Write)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // 5: Task State Segment (TSS) (0x28)
    uint32_t kstack_top = (uint32_t)default_kernel_stack + sizeof(default_kernel_stack);
    write_tss(5, KERNEL_DS_SEL, kstack_top);

    // Load GDT & flush segments
    gdt_flush_asm((uint32_t)&gdt_p);
    tss_flush_asm();

    serial_println("[GDT] GDT initialized with Kernel & User segments and active TSS.");
}

void gdt_set_kernel_stack(uint32_t stack_top) {
    tss.esp0 = stack_top;
}

uint32_t gdt_get_kernel_stack(void) {
    return tss.esp0;
}
