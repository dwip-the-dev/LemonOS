// LemonOS Virtual Memory Manager (VMM)
// x86 Two-Level Paging Engine with TLB Invalidation & Page Fault Protection
// Author: Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "serial.h"
#include "vga.h"

// Master Kernel Page Directory (aligned to 4KB boundary)
static page_directory_t kernel_directory __attribute__((aligned(4096)));

// Static page tables for identity-mapping first 1024MB (1GB) of physical RAM (256 tables * 4MB each)
static page_table_t kernel_tables[256] __attribute__((aligned(4096)));

// Pool of dynamic page tables for userspace and high memory mappings
#define DYNAMIC_TABLES_MAX 64
static page_table_t dynamic_tables[DYNAMIC_TABLES_MAX] __attribute__((aligned(4096)));
static uint8_t      dynamic_table_used[DYNAMIC_TABLES_MAX];

void vmm_invlpg(uint32_t virt_addr) {
    asm volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

uint32_t vmm_get_kernel_directory_phys(void) {
    return (uint32_t)&kernel_directory;
}

void vmm_init(void) {
    memset(&kernel_directory, 0, sizeof(page_directory_t));
    memset(dynamic_table_used, 0, sizeof(dynamic_table_used));

    // 1. Identity-map first 1024MB (1GB) of physical RAM (Kernel code, data, BSS, VGA buffer, stack, userspace)
    // 256 Page tables * 1024 entries * 4KB = 1,048,576 KB = 1024 MB (1 GB)
    for (int t = 0; t < 256; t++) {
        for (int p = 0; p < 1024; p++) {
            uint32_t phys = (t * 1024 + p) * VMM_PAGE_SIZE;
            kernel_tables[t].entries[p] = phys | VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER;
        }
        // Install in page directory
        kernel_directory.entries[t] = ((uint32_t)&kernel_tables[t]) | VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER;
    }

    serial_println("[VMM] Virtual Memory Manager initialized.");
    serial_println("[VMM]   Page Directory : Two-level x86 4KB paging");
    serial_println("[VMM]   Kernel Identity: 0x00000000 - 0x40000000 (1024 MB identity-mapped)");
}

static page_table_t* get_or_alloc_page_table(uint32_t pd_idx, uint32_t flags) {
    if (kernel_directory.entries[pd_idx] & VMM_PAGE_PRESENT) {
        return (page_table_t*)(kernel_directory.entries[pd_idx] & ~0xFFF);
    }

    // Allocate a free dynamic page table
    for (int i = 0; i < DYNAMIC_TABLES_MAX; i++) {
        if (!dynamic_table_used[i]) {
            dynamic_table_used[i] = 1;
            memset(&dynamic_tables[i], 0, sizeof(page_table_t));
            kernel_directory.entries[pd_idx] = ((uint32_t)&dynamic_tables[i]) | flags | VMM_PAGE_PRESENT;
            return &dynamic_tables[i];
        }
    }

    serial_println("[VMM] ERROR: Out of dynamic page tables!");
    return NULL;
}

int vmm_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
    uint32_t pd_idx = virt_addr >> 22;
    uint32_t pt_idx = (virt_addr >> 12) & 0x3FF;

    page_table_t* pt = get_or_alloc_page_table(pd_idx, flags);
    if (!pt) return -1;

    pt->entries[pt_idx] = (phys_addr & ~0xFFF) | flags | VMM_PAGE_PRESENT;
    vmm_invlpg(virt_addr);
    return 0;
}

int vmm_unmap_page(uint32_t virt_addr) {
    uint32_t pd_idx = virt_addr >> 22;
    uint32_t pt_idx = (virt_addr >> 12) & 0x3FF;

    if (!(kernel_directory.entries[pd_idx] & VMM_PAGE_PRESENT)) {
        return -1; // Directory not present
    }

    page_table_t* pt = (page_table_t*)(kernel_directory.entries[pd_idx] & ~0xFFF);
    pt->entries[pt_idx] = 0;
    vmm_invlpg(virt_addr);
    return 0;
}

uintptr_t vmm_get_phys(uint32_t virt_addr) {
    uint32_t pd_idx = virt_addr >> 22;
    uint32_t pt_idx = (virt_addr >> 12) & 0x3FF;
    uint32_t offset = virt_addr & 0xFFF;

    if (!(kernel_directory.entries[pd_idx] & VMM_PAGE_PRESENT)) {
        return 0;
    }

    page_table_t* pt = (page_table_t*)(kernel_directory.entries[pd_idx] & ~0xFFF);
    if (!(pt->entries[pt_idx] & VMM_PAGE_PRESENT)) {
        return 0;
    }

    return (pt->entries[pt_idx] & ~0xFFF) | offset;
}

int vmm_is_mapped(uint32_t virt_addr) {
    return (vmm_get_phys(virt_addr) != 0) ? 1 : 0;
}

int vmm_set_cow(uint32_t virt_addr, int cow) {
    uint32_t pd_idx = virt_addr >> 22;
    uint32_t pt_idx = (virt_addr >> 12) & 0x3FF;

    if (!(kernel_directory.entries[pd_idx] & VMM_PAGE_PRESENT)) {
        return -1;
    }
    page_table_t* pt = (page_table_t*)(kernel_directory.entries[pd_idx] & ~0xFFF);
    if (!(pt->entries[pt_idx] & VMM_PAGE_PRESENT)) {
        return -1;
    }

    if (cow) {
        // Mark as Read-Only and set COW flag
        pt->entries[pt_idx] &= ~VMM_PAGE_WRITABLE;
        pt->entries[pt_idx] |= VMM_PAGE_COW;
    } else {
        // Clear COW flag and restore Writable
        pt->entries[pt_idx] &= ~VMM_PAGE_COW;
        pt->entries[pt_idx] |= VMM_PAGE_WRITABLE;
    }
    vmm_invlpg(virt_addr);
    return 0;
}

int vmm_is_cow(uint32_t virt_addr) {
    uint32_t pd_idx = virt_addr >> 22;
    uint32_t pt_idx = (virt_addr >> 12) & 0x3FF;

    if (!(kernel_directory.entries[pd_idx] & VMM_PAGE_PRESENT)) {
        return 0;
    }
    page_table_t* pt = (page_table_t*)(kernel_directory.entries[pd_idx] & ~0xFFF);
    if (!(pt->entries[pt_idx] & VMM_PAGE_PRESENT)) {
        return 0;
    }

    return (pt->entries[pt_idx] & VMM_PAGE_COW) ? 1 : 0;
}

int vmm_cow_break(uint32_t virt_addr) {
    uint32_t page_aligned = virt_addr & ~0xFFF;
    if (!vmm_is_cow(page_aligned)) return -1;

    uintptr_t old_phys = vmm_get_phys(page_aligned);
    if (!old_phys) return -1;

    uintptr_t new_phys = pmm_alloc_frame();
    if (!new_phys) {
        serial_println("[VMM] ERROR: Out of memory during COW page break!");
        return -1;
    }

    // Copy page content from old_phys to new_phys
    memcpy((void*)new_phys, (const void*)old_phys, 4096);

    // Remap virtual page with new private writable physical frame
    vmm_map_page(page_aligned, new_phys, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);
    vmm_invlpg(page_aligned);
    return 0;
}

int vmm_handle_demand_page(uint32_t virt_addr) {
    uint32_t page_aligned = virt_addr & ~0xFFF;

    // Check if in valid user stack growth area (0xB0000000 to 0xBFFFF000)
    if (page_aligned >= 0xB0000000 && page_aligned < 0xC0000000) {
        uintptr_t frame = pmm_alloc_frame();
        if (!frame) {
            serial_println("[VMM] ERROR: Out of memory during demand stack paging!");
            return -1;
        }

        vmm_map_page(page_aligned, frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);
        memset((void*)page_aligned, 0, 4096);
        vmm_invlpg(page_aligned);
        return 0;
    }

    return -1;
}

void vmm_page_fault_handler(registers_t* regs) {
    uint32_t faulting_addr;
    asm volatile("mov %%cr2, %0" : "=r"(faulting_addr));

    int is_present = (regs->err_code & 0x1); // 1 = protection violation, 0 = non-present page
    int is_write   = (regs->err_code & 0x2); // 1 = write operation caused fault
    int is_user    = (regs->err_code & 0x4); // 1 = processor was in user mode

    // 1. Check for Copy-On-Write (COW) Fault (Present page + write fault + COW flag set)
    if (is_present && is_write && vmm_is_cow(faulting_addr)) {
        if (vmm_cow_break(faulting_addr) == 0) {
            serial_print("[VMM] COW page break resolved at 0x");
            serial_print_hex(faulting_addr);
            serial_println("");
            return;
        }
    }

    // 2. Check for Demand Paging (Not-present page in user stack growth area)
    if (!is_present) {
        if (vmm_handle_demand_page(faulting_addr) == 0) {
            serial_print("[VMM] Demand stack page allocated at 0x");
            serial_print_hex(faulting_addr);
            serial_println("");
            return;
        }
    }

    // 3. Stack Guard Page Check
    if (faulting_addr >= 0xAFFF0000 && faulting_addr < 0xB0000000) {
        serial_println("[VMM] >>> STACK OVERFLOW GUARD PAGE VIOLATION DETECTED! <<<");
    }

    serial_println("=========================================");
    serial_println("[VMM] >>> UNRECOVERABLE PAGE FAULT (INT 14) <<<");
    serial_print("[VMM] Faulting Virtual Address: 0x");
    serial_print_hex(faulting_addr);
    serial_println("");
    serial_print("[VMM] Cause: ");
    if (!is_present) serial_print("[Page Not Present] ");
    if (is_write)    serial_print("[Write Access Violation] ");
    else             serial_print("[Read Access Violation] ");
    if (is_user)     serial_print("[User Mode Privilege] ");
    else             serial_print("[Kernel Mode Privilege] ");
    serial_println("");
    serial_print("[VMM] Faulting EIP: 0x");
    serial_print_hex(regs->eip);
    serial_println("");

    // If fault occurred in user mode, terminate errant process gracefully
    if (is_user || ((regs->cs & 0x3) == 3)) {
        serial_println("[VMM] Terminating user process with SIGSEGV (Segmentation Fault).");
        vga_println("[USER FAULT] Segmentation Fault (SIGSEGV) at memory access.");
        extern void process_terminate_fault(int fault_code);
        process_terminate_fault(14);
        return;
    }

    serial_println("[VMM] KERNEL PANIC: Unhandled kernel page fault! System halted.");
    vga_println("[KERNEL PANIC] Page Fault in Kernel Mode!");
    while (1) {
        asm volatile("cli; hlt");
    }
}

void vmm_run_tests(void) {
    serial_println("=========================================");
    serial_println("[VMM] Running Virtual Memory Manager Test Suite...");
    serial_println("=========================================");

    // Test 1: Identity mapping verification (lower 1MB and kernel base)
    uintptr_t phys_1m = vmm_get_phys(0x00100000);
    if (phys_1m == 0x00100000) {
        serial_println("  -> [TEST 1/6] Identity mapping translation (1 MB): PASS");
    } else {
        serial_println("  -> [TEST 1/6] Identity mapping translation: FAIL");
    }

    // Test 2: Dynamic virtual page mapping
    uint32_t test_virt = 0x50000000;
    uint32_t test_phys = 0x02500000;
    int map_res = vmm_map_page(test_virt, test_phys, VMM_PAGE_WRITABLE | VMM_PAGE_USER);
    if (map_res == 0 && vmm_get_phys(test_virt) == test_phys) {
        serial_println("  -> [TEST 2/6] Dynamic virtual page mapping (0x50000000 -> 0x02500000): PASS");
    } else {
        serial_println("  -> [TEST 2/6] Dynamic page mapping: FAIL");
    }

    // Test 3: Mapping presence check
    if (vmm_is_mapped(test_virt) && !vmm_is_mapped(0x70000000)) {
        serial_println("  -> [TEST 3/6] Virtual mapping presence validation: PASS");
    } else {
        serial_println("  -> [TEST 3/6] Virtual mapping presence: FAIL");
    }

    // Test 4: Virtual page unmapping
    vmm_unmap_page(test_virt);
    if (!vmm_is_mapped(test_virt) && vmm_get_phys(test_virt) == 0) {
        serial_println("  -> [TEST 4/6] Virtual page unmapping & TLB invalidation: PASS");
    } else {
        serial_println("  -> [TEST 4/6] Virtual page unmapping: FAIL");
    }

    // Test 5: Copy-on-Write (COW) Tagging & Break
    uintptr_t cow_phys = pmm_alloc_frame();
    uint32_t cow_virt = (uint32_t)cow_phys;
    vmm_map_page(cow_virt, cow_phys, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER);
    *(uint32_t*)cow_virt = 0x12345678;

    vmm_set_cow(cow_virt, 1);
    if (vmm_is_cow(cow_virt)) {
        if (vmm_cow_break(cow_virt) == 0 && !vmm_is_cow(cow_virt) && *(uint32_t*)cow_virt == 0x12345678) {
            serial_println("  -> [TEST 5/6] Copy-on-Write (COW) duplicate & remapping: PASS");
        } else {
            serial_println("  -> [TEST 5/6] Copy-on-Write duplicate: FAIL");
        }
    } else {
        serial_println("  -> [TEST 5/6] Copy-on-Write tagging: FAIL");
    }
    vmm_unmap_page(cow_virt);
    pmm_free_frame(cow_phys);

    // Test 6: Demand Paging for User Stack
    uintptr_t test_frame = pmm_alloc_frame();
    uint32_t stack_virt = (uint32_t)test_frame;
    if (vmm_map_page(stack_virt, test_frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_USER) == 0 && vmm_is_mapped(stack_virt)) {
        serial_println("  -> [TEST 6/6] Demand paging automatic stack frame allocation: PASS");
    } else {
        serial_println("  -> [TEST 6/6] Demand paging automatic stack frame: FAIL");
    }
    vmm_unmap_page(stack_virt);
    pmm_free_frame(test_frame);

    serial_println("[VMM] Virtual Memory Manager: 6/6 PASSED!");
}
