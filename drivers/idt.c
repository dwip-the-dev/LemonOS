#include "idt.h"
#include "gdt.h"
#include "syscall.h"
#include "serial.h"
#include "vga.h"
#include <stdint.h>
#include <stddef.h>

static idt_entry_t idt_entries[IDT_NUM_ENTRIES];
static idt_ptr_t   idt_p;

extern void idt_load_asm(uint32_t idt_ptr_addr);
extern void syscall_entry_asm(void);

// External ISR stubs declared in sys_asm.s
extern void isr_stub_0(void);
extern void isr_stub_1(void);
extern void isr_stub_2(void);
extern void isr_stub_3(void);
extern void isr_stub_4(void);
extern void isr_stub_5(void);
extern void isr_stub_6(void);
extern void isr_stub_7(void);
extern void isr_stub_8(void);
extern void isr_stub_9(void);
extern void isr_stub_10(void);
extern void isr_stub_11(void);
extern void isr_stub_12(void);
extern void isr_stub_13(void);
extern void isr_stub_14(void);
extern void isr_stub_15(void);
extern void isr_stub_16(void);
extern void isr_stub_17(void);
extern void isr_stub_18(void);
extern void isr_stub_19(void);
extern void isr_stub_20(void);
extern void isr_stub_21(void);
extern void isr_stub_22(void);
extern void isr_stub_23(void);
extern void isr_stub_24(void);
extern void isr_stub_25(void);
extern void isr_stub_26(void);
extern void isr_stub_27(void);
extern void isr_stub_28(void);
extern void isr_stub_29(void);
extern void isr_stub_30(void);
extern void isr_stub_31(void);

typedef void (*isr_t)(void);
static const isr_t isr_stubs[32] = {
    isr_stub_0,  isr_stub_1,  isr_stub_2,  isr_stub_3,
    isr_stub_4,  isr_stub_5,  isr_stub_6,  isr_stub_7,
    isr_stub_8,  isr_stub_9,  isr_stub_10, isr_stub_11,
    isr_stub_12, isr_stub_13, isr_stub_14, isr_stub_15,
    isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19,
    isr_stub_20, isr_stub_21, isr_stub_22, isr_stub_23,
    isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27,
    isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31
};

static const char* exception_names[32] = {
    "Divide by Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection", "VMM Communication", "Security Exception", "Reserved"
};

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_low  = (base & 0xFFFF);
    idt_entries[num].sel       = sel;
    idt_entries[num].always0   = 0;
    idt_entries[num].flags     = flags;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
}

static inline void outb_pic(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void idt_init(void) {
    serial_println("[IDT] Initializing Interrupt Descriptor Table...");

    // Mask all IRQs on 8259 PIC (Master and Slave) to prevent spurious IRQ8/Double Fault
    outb_pic(0x21, 0xFF);
    outb_pic(0xA1, 0xFF);

    idt_p.limit = (sizeof(idt_entry_t) * IDT_NUM_ENTRIES) - 1;
    idt_p.base  = (uint32_t)&idt_entries;

    // Clear all entries
    for (int i = 0; i < IDT_NUM_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // Set CPU exception gates 0..31 (Ring 0)
    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, (uint32_t)isr_stubs[i], KERNEL_CS_SEL, IDT_GATE_KERNEL_INT);
    }

    // Set Syscall Gate 0x80 (Ring 3 accessible, DPL=3)
    idt_set_gate(0x80, (uint32_t)syscall_entry_asm, KERNEL_CS_SEL, IDT_GATE_USER_INT);

    // Load IDT
    idt_load_asm((uint32_t)&idt_p);

    serial_println("[IDT] IDT loaded with 32 CPU exceptions and 0x80 Ring 3 Syscall Gate.");
}

void exception_handler(registers_t* regs) {
    uint32_t int_no = regs->int_no;

    // Route Page Faults (INT 14) to VMM handler for Demand Paging & Copy-on-Write
    if (int_no == 14) {
        extern void vmm_page_fault_handler(registers_t* regs);
        vmm_page_fault_handler(regs);
        return;
    }

    const char* name = (int_no < 32) ? exception_names[int_no] : "Unknown Interrupt";

    serial_println("=========================================");
    serial_print("[CPU FAULT] Exception #");
    serial_print_dec(int_no);
    serial_print(" (");
    serial_print(name);
    serial_println(")");
    serial_print("  EIP: 0x"); serial_print_hex(regs->eip);
    serial_print("  CS:  0x"); serial_print_hex(regs->cs);
    serial_print("  EFLAGS: 0x"); serial_print_hex(regs->eflags);
    serial_println("");
    serial_print("  Error Code: 0x"); serial_print_hex(regs->err_code);
    serial_println("");

    // If fault occurred in User Mode (Ring 3)
    if ((regs->cs & 0x03) == 3) {
        serial_println("[CPU FAULT] Fault occurred in User Mode (Ring 3). Terminating process.");
        vga_print("[USERSPACE FAULT] Process terminated due to ");
        vga_println(name);

        // Terminate and exit back to kernel shell
        extern void process_terminate_fault(int fault_code);
        process_terminate_fault(int_no);
        return;
    }

    serial_println("[CPU FAULT] Kernel mode exception! System halting.");
    vga_print("[KERNEL PANIC] Exception #");
    vga_print(name);
    while (1) {
        asm volatile("cli; hlt");
    }
}
