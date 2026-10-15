#ifndef USER_LOADER_H
#define USER_LOADER_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PROCESSES           8
#define USER_STACK_SIZE         (128 * 1024)  // 128 KB user stack
#define KERNEL_STACK_SIZE       (32 * 1024)   // 32 KB kernel stack per process
#define USER_MEMORY_BASE        0x02000000    // 32 MB base address (above kernel BSS)
#define USER_PROCESS_MEM_SIZE   (8 * 1024 * 1024) // 8 MB per process

// Process States
#define PROC_STATE_UNUSED       0
#define PROC_STATE_READY        1
#define PROC_STATE_RUNNING      2
#define PROC_STATE_TERMINATED   3

// ELF32 Types & Constants
#define ELF_MAGIC               0x464C457F    // "\x7fELF"
#define ET_NONE                 0
#define ET_REL                  1
#define ET_EXEC                 2
#define ET_DYN                  3
#define ET_CORE                 4
#define EM_386                  3
#define PT_LOAD                 1

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf32_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;

// Lemon Executable (LEXE) Binary Header
#define LEXE_MAGIC              0x4C455845    // "LEXE"
#define LEXE_VERSION            1

typedef struct {
    uint32_t magic;         // LEXE_MAGIC
    uint32_t version;       // LEXE_VERSION
    uint32_t entry_offset;  // Entry point offset from base
    uint32_t text_size;     // Executable text segment size
    uint32_t data_size;     // Initialized data size
    uint32_t bss_size;      // Uninitialized bss size
    uint32_t heap_size;     // Initial heap size
    uint32_t stack_size;    // Stack size
} __attribute__((packed)) lexe_header_t;

// Process Control Block (PCB)
typedef struct {
    int      pid;
    int      ppid;
    char     name[32];
    char     cwd[128];
    uint32_t entry_point;

    // Memory regions
    uint32_t user_memory_base;
    uint32_t user_memory_size;
    uint32_t user_stack_top;
    uint32_t kernel_stack_top;

    // Heap bounds (sbrk)
    uint32_t heap_start;
    uint32_t heap_break;
    uint32_t heap_max;

    int      state;
    int      exit_code;
    int      slot;
} process_t;

// Userspace Loader API
void       user_loader_init(void);
int        process_spawn(const char* path, int argc, char** argv);
int        process_spawn_env(const char* path, int argc, char** argv, char** envp);
int        process_execve(const char* path, char** argv, char** envp);
int        process_fork_user(void);
process_t* process_get_current(void);
process_t* process_get_by_pid(int pid);
void       process_exit(int code);
void       process_list(void);
void       user_loader_run_tests(void);

// Transition to Ring 3 (User Mode)
void jump_to_ring3(uint32_t entry, uint32_t user_stack, int argc, char** argv);

#endif
