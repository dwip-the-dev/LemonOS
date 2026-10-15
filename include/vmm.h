#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include "syscall.h"

#ifdef __cplusplus
extern "C" {
#endif

// Page Table / Directory Entry Flags
#define VMM_PAGE_PRESENT        0x001
#define VMM_PAGE_WRITABLE       0x002
#define VMM_PAGE_USER           0x004
#define VMM_PAGE_WRITETHROUGH   0x008
#define VMM_PAGE_NOCACHE        0x010
#define VMM_PAGE_ACCESSED       0x020
#define VMM_PAGE_DIRTY          0x040
#define VMM_PAGE_4MB            0x080
#define VMM_PAGE_GLOBAL         0x100
#define VMM_PAGE_COW            0x200  // Copy-On-Write (PTE bit 9, available for OS)

// Total entries in an x86 Page Directory and Page Table
#define VMM_ENTRIES_PER_TABLE   1024
#define VMM_PAGE_SIZE           4096

// Page Directory and Page Table Types
typedef uint32_t page_directory_entry_t;
typedef uint32_t page_table_entry_t;

typedef struct {
    page_table_entry_t entries[VMM_ENTRIES_PER_TABLE];
} __attribute__((aligned(4096))) page_table_t;

typedef struct {
    page_directory_entry_t entries[VMM_ENTRIES_PER_TABLE];
} __attribute__((aligned(4096))) page_directory_t;

// Initialize Virtual Memory Manager & Kernel Master Page Directory
void      vmm_init(void);

// Map virtual address to physical address in active page directory
int       vmm_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);

// Unmap a virtual page and flush TLB
int       vmm_unmap_page(uint32_t virt_addr);

// Translate virtual address to physical address (returns 0 if not mapped)
uintptr_t vmm_get_phys(uint32_t virt_addr);

// Check if virtual address is mapped
int       vmm_is_mapped(uint32_t virt_addr);

// Copy-on-Write Subsystem
int       vmm_set_cow(uint32_t virt_addr, int cow);
int       vmm_is_cow(uint32_t virt_addr);
int       vmm_cow_break(uint32_t virt_addr);

// Demand Paging Engine (Automatic stack expansion)
int       vmm_handle_demand_page(uint32_t virt_addr);

// Flush a single TLB entry
void      vmm_invlpg(uint32_t virt_addr);

// Get kernel page directory physical base address
uint32_t  vmm_get_kernel_directory_phys(void);

// Page Fault (INT 14) Exception Handler
void      vmm_page_fault_handler(registers_t* regs);

// Self-Test Suite
void      vmm_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif // VMM_H
