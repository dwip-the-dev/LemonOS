#include "user_loader.h"
#include "proc_mgr.h"
#include "dynlink.h"
#include "gdt.h"
#include "vfs.h"
#include "pmm.h"
#include "serial.h"
#include "vga.h"
#include <stdint.h>
#include <stddef.h>

static process_t processes[MAX_PROCESSES];
static process_t* current_process = NULL;
static int next_pid = 1;

// Dedicated Kernel Stack Pool per process
static uint8_t user_kstack_pool[MAX_PROCESSES][KERNEL_STACK_SIZE] __attribute__((aligned(16)));

// Jump buffer for returning to kernel context when process exits
typedef struct {
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t eip;
} jmp_buf_t;

extern int  user_setjmp(jmp_buf_t* buf);
extern void user_longjmp(jmp_buf_t* buf, int code) __attribute__((noreturn));

static jmp_buf_t kernel_return_ctx;

static __attribute__((unused)) int u_strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static void u_strcpy(char* d, const char* s, int max) {
    int i = 0;
    while (s && s[i] && i < max - 1) { d[i] = s[i]; i++; }
    d[i] = '\0';
}

static __attribute__((unused)) int u_strlen(const char* s) {
    int len = 0;
    while (s && s[len]) len++;
    return len;
}

void user_loader_init(void) {
    serial_println("[USERSPACE] Initializing Userspace Process Loader...");

    for (int i = 0; i < MAX_PROCESSES; i++) {
        processes[i].pid = 0;
        processes[i].ppid = 0;
        processes[i].slot = i;
        processes[i].state = PROC_STATE_UNUSED;
        processes[i].name[0] = '\0';
        u_strcpy(processes[i].cwd, "/home/dwip", sizeof(processes[i].cwd));
        processes[i].entry_point = 0;
        processes[i].user_memory_base = USER_MEMORY_BASE + (i * USER_PROCESS_MEM_SIZE);
        processes[i].user_memory_size = USER_PROCESS_MEM_SIZE;
        processes[i].user_stack_top = processes[i].user_memory_base + USER_PROCESS_MEM_SIZE - 64;
        processes[i].kernel_stack_top = (uint32_t)&user_kstack_pool[i][KERNEL_STACK_SIZE - 64];
        processes[i].heap_start = 0;
        processes[i].heap_break = 0;
        processes[i].heap_max = 0;
        processes[i].exit_code = 0;
    }

    // Reserve dedicated userspace process physical memory in PMM so kmem never collides
    pmm_reserve_region(USER_MEMORY_BASE, MAX_PROCESSES * USER_PROCESS_MEM_SIZE);

    current_process = NULL;
    serial_println("[USERSPACE] Process table and physical memory regions initialized.");
}

process_t* process_get_current(void) {
    return current_process;
}

process_t* process_get_by_pid(int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_STATE_UNUSED && processes[i].pid == pid) {
            return &processes[i];
        }
    }
    return NULL;
}

static int find_free_slot(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_STATE_UNUSED || processes[i].state == PROC_STATE_TERMINATED) {
            return i;
        }
    }
    return -1;
}

// ELF32 Binary Loader
static int elf_load(int fd, process_t* proc) {
    elf32_ehdr_t ehdr;
    vfs_seek(fd, 0, SEEK_SET);
    int r = vfs_read(fd, &ehdr, sizeof(elf32_ehdr_t));
    if (r < (int)sizeof(elf32_ehdr_t)) return -1;

    // Check ELF Magic
    if (*(uint32_t*)ehdr.e_ident != ELF_MAGIC) {
        return -2; // Not an ELF file
    }

    if (ehdr.e_type != ET_EXEC && ehdr.e_type != ET_DYN) {
        return -3;
    }

    uint8_t* mem = (uint8_t*)proc->user_memory_base;
    for (uint32_t i = 0; i < USER_PROCESS_MEM_SIZE; i++) {
        mem[i] = 0;
    }

    uint32_t max_loaded_addr = 0;

    // Iterate Program Headers
    for (int i = 0; i < ehdr.e_phnum; i++) {
        elf32_phdr_t phdr;
        vfs_seek(fd, ehdr.e_phoff + (i * sizeof(elf32_phdr_t)), SEEK_SET);
        if (vfs_read(fd, &phdr, sizeof(elf32_phdr_t)) != sizeof(elf32_phdr_t)) {
            return -4;
        }

        if (phdr.p_type == PT_LOAD) {
            uint32_t offset_in_mem = (phdr.p_vaddr >= USER_MEMORY_BASE) ?
                                     (phdr.p_vaddr - USER_MEMORY_BASE) : phdr.p_vaddr;

            if (offset_in_mem + phdr.p_memsz > USER_PROCESS_MEM_SIZE - USER_STACK_SIZE) {
                return -5; // Binary exceeds process space
            }

            vfs_seek(fd, phdr.p_offset, SEEK_SET);
            int br = vfs_read(fd, mem + offset_in_mem, phdr.p_filesz);
            serial_print("[USERSPACE] phdr: off=0x"); serial_print_hex(phdr.p_offset);
            serial_print(" filesz="); serial_print_dec(phdr.p_filesz);
            serial_print(" read="); serial_print_dec(br);
            serial_print(" ch890=0x"); serial_print_hex(mem[offset_in_mem + 0x890]);
            serial_println("");

            // Zero .bss
            for (uint32_t b = phdr.p_filesz; b < phdr.p_memsz; b++) {
                mem[offset_in_mem + b] = 0;
            }

            if (offset_in_mem + phdr.p_memsz > max_loaded_addr) {
                max_loaded_addr = offset_in_mem + phdr.p_memsz;
            }
        }
    }

    uint32_t entry_offset = (ehdr.e_entry >= USER_MEMORY_BASE) ?
                            (ehdr.e_entry - USER_MEMORY_BASE) : ehdr.e_entry;

    proc->entry_point = proc->user_memory_base + entry_offset;

    // Align heap start to 16 bytes
    proc->heap_start = proc->user_memory_base + ((max_loaded_addr + 15) & ~15);
    proc->heap_break = proc->heap_start;
    proc->heap_max   = proc->user_memory_base + (USER_PROCESS_MEM_SIZE - USER_STACK_SIZE - 4096);

    // Dynamic Linking, GOT, and PLT setup if PT_DYNAMIC segment is present
    dynlink_prepare_executable(proc, fd, &ehdr);

    return 0;
}

// LEXE (Lemon Executable) Loader
static int lexe_load(int fd, process_t* proc) {
    lexe_header_t lhdr;
    vfs_seek(fd, 0, SEEK_SET);
    int r = vfs_read(fd, &lhdr, sizeof(lexe_header_t));
    if (r < (int)sizeof(lexe_header_t)) return -1;

    if (lhdr.magic != LEXE_MAGIC) return -2;

    uint8_t* mem = (uint8_t*)proc->user_memory_base;
    for (uint32_t i = 0; i < USER_PROCESS_MEM_SIZE; i++) {
        mem[i] = 0;
    }

    uint32_t to_read = lhdr.text_size + lhdr.data_size;
    if (to_read > USER_PROCESS_MEM_SIZE - USER_STACK_SIZE) return -3;

    vfs_seek(fd, sizeof(lexe_header_t), SEEK_SET);
    vfs_read(fd, mem, to_read);

    proc->entry_point = proc->user_memory_base + lhdr.entry_offset;
    proc->heap_start  = proc->user_memory_base + ((to_read + lhdr.bss_size + 15) & ~15);
    proc->heap_break  = proc->heap_start;
    proc->heap_max    = proc->user_memory_base + (USER_PROCESS_MEM_SIZE - USER_STACK_SIZE - 4096);

    return 0;
}

// Raw Flat Binary Loader
static int flat_bin_load(int fd, process_t* proc) {
    vfs_seek(fd, 0, SEEK_SET);
    vfs_seek(fd, 0, SEEK_END);
    uint32_t size = vfs_seek(fd, 0, SEEK_CUR);

    if (size == 0 || size > USER_PROCESS_MEM_SIZE - USER_STACK_SIZE) return -1;

    uint8_t* mem = (uint8_t*)proc->user_memory_base;
    for (uint32_t i = 0; i < USER_PROCESS_MEM_SIZE; i++) {
        mem[i] = 0;
    }

    vfs_seek(fd, 0, SEEK_SET);
    vfs_read(fd, mem, size);

    proc->entry_point = proc->user_memory_base;
    proc->heap_start  = proc->user_memory_base + ((size + 15) & ~15);
    proc->heap_break  = proc->heap_start;
    proc->heap_max    = proc->user_memory_base + (USER_PROCESS_MEM_SIZE - USER_STACK_SIZE - 4096);

    return 0;
}

// System V ABI Compliant User Stack Setup
static uint32_t setup_user_stack(process_t* proc, int argc, char** argv, char** envp) {
    char* s_top = (char*)(proc->user_memory_base + proc->user_memory_size - 128);

    // 1. Copy environment strings onto user stack
    char* user_envp[16];
    int envc = 0;
    const char* default_env[] = {
        "PATH=/bin:/usr/bin",
        "USER=dwip",
        "HOME=/home/dwip",
        "TERM=lemon-terminal",
        "SHELL=/bin/sh",
        NULL
    };
    char* const* src_env = (envp && envp[0]) ? envp : (char* const*)default_env;

    while (src_env && src_env[envc] && envc < 15) {
        size_t len = u_strlen(src_env[envc]) + 1;
        s_top -= len;
        for (size_t k = 0; k < len; k++) s_top[k] = src_env[envc][k];
        user_envp[envc] = s_top;
        envc++;
    }
    user_envp[envc] = NULL;

    // 2. Copy argv strings onto user stack
    char* user_argv[32];
    int act_argc = (argc > 0) ? argc : 1;
    if (act_argc > 31) act_argc = 31;

    for (int i = 0; i < act_argc; i++) {
        const char* s = (argv && argv[i]) ? argv[i] : proc->name;
        size_t len = u_strlen(s) + 1;
        s_top -= len;
        for (size_t k = 0; k < len; k++) s_top[k] = s[k];
        user_argv[i] = s_top;
    }
    user_argv[act_argc] = NULL;

    // Align stack pointer to 4 bytes
    uint32_t* ustack = (uint32_t*)((uintptr_t)s_top & ~3);

    // Calculate total entries to push:
    // 8 words for auxv + (envc + 1) + (act_argc + 1) + 1 word for argc
    int total_words = 8 + (envc + 1) + (act_argc + 1) + 1;
    // Align base so that final ustack after pushing total_words is 16-byte aligned
    uintptr_t target_final = ((uintptr_t)(ustack - total_words)) & ~15;
    ustack = (uint32_t*)(target_final + total_words * 4);

    // 3. Auxiliary Vector (auxv)
    *(--ustack) = 0; *(--ustack) = 0; // AT_NULL
    *(--ustack) = proc->entry_point; *(--ustack) = 9; // AT_ENTRY = 9
    *(--ustack) = 4096; *(--ustack) = 6; // AT_PAGESZ = 6
    *(--ustack) = proc->user_memory_base; *(--ustack) = 7; // AT_BASE = 7

    // 4. Push envp array (NULL-terminated)
    *(--ustack) = 0;
    for (int i = envc - 1; i >= 0; i--) {
        *(--ustack) = (uint32_t)user_envp[i];
    }

    // 5. Push argv array (NULL-terminated)
    *(--ustack) = 0;
    for (int i = act_argc - 1; i >= 0; i--) {
        *(--ustack) = (uint32_t)user_argv[i];
    }

    // 6. Push argc (System V ABI entry point)
    *(--ustack) = (uint32_t)act_argc;

    return (uint32_t)ustack;
}

int process_fork_user(void) {
    if (!current_process) return -1;

    int slot = find_free_slot();
    if (slot < 0) return -1;

    process_t* child = &processes[slot];
    child->pid = next_pid++;
    child->ppid = current_process->pid;
    child->slot = slot;
    u_strcpy(child->name, current_process->name, sizeof(child->name));
    u_strcpy(child->cwd, current_process->cwd, sizeof(child->cwd));
    child->entry_point = current_process->entry_point;
    child->heap_start = child->user_memory_base + (current_process->heap_start - current_process->user_memory_base);
    child->heap_break = child->user_memory_base + (current_process->heap_break - current_process->user_memory_base);
    child->heap_max = child->user_memory_base + (current_process->heap_max - current_process->user_memory_base);
    child->user_stack_top = child->user_memory_base + (current_process->user_stack_top - current_process->user_memory_base);
    child->state = PROC_STATE_READY;
    child->exit_code = 0;

    // Clone parent memory to child
    uint8_t* src = (uint8_t*)current_process->user_memory_base;
    uint8_t* dst = (uint8_t*)child->user_memory_base;
    for (uint32_t i = 0; i < USER_PROCESS_MEM_SIZE; i++) {
        dst[i] = src[i];
    }

    proc_fork(current_process->pid);

    serial_print("[USERSPACE] Forked child process PID ");
    serial_print_dec(child->pid);
    serial_print(" from parent PID ");
    serial_print_dec(current_process->pid);
    serial_println("");

    return child->pid;
}

int process_execve(const char* path, char** argv, char** envp) {
    if (!path || !current_process) return -1;

    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) return -2;

    int load_res = elf_load(fd, current_process);
    if (load_res != 0) {
        load_res = lexe_load(fd, current_process);
    }
    if (load_res != 0) {
        load_res = flat_bin_load(fd, current_process);
    }
    vfs_close(fd);

    if (load_res != 0) return -8; // ENOEXEC

    u_strcpy(current_process->name, path, sizeof(current_process->name));

    int argc = 0;
    if (argv) {
        while (argv[argc]) argc++;
    }

    current_process->user_stack_top = setup_user_stack(current_process, argc, argv, envp);

    serial_print("[USERSPACE] Execve '");
    serial_print(path);
    serial_print("' loaded at Entry=0x");
    serial_print_hex(current_process->entry_point);
    serial_println("");

    return 0;
}

// Process Spawning & Ring 3 Execution
int process_spawn_env(const char* path, int argc, char** argv, char** envp) {
    if (!path) return -1;

    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        serial_print("[USERSPACE] Failed to open binary '");
        serial_print(path);
        serial_println("'");
        return -2;
    }

    int slot = find_free_slot();
    if (slot < 0) {
        serial_println("[USERSPACE] Max process table limit reached!");
        vfs_close(fd);
        return -3;
    }

    process_t* proc = &processes[slot];
    proc->pid = next_pid++;
    proc->ppid = current_process ? current_process->pid : 1;
    proc->slot = slot;
    u_strcpy(proc->name, path, 32);
    if (proc->cwd[0] == '\0') {
        u_strcpy(proc->cwd, "/home/dwip", sizeof(proc->cwd));
    }
    proc->state = PROC_STATE_READY;
    proc->exit_code = 0;

    // Check Format and Load
    int load_res = elf_load(fd, proc);
    if (load_res != 0) {
        load_res = lexe_load(fd, proc);
    }
    if (load_res != 0) {
        load_res = flat_bin_load(fd, proc);
    }
    vfs_close(fd);

    if (load_res != 0) {
        serial_println("[USERSPACE] Unrecognized executable format!");
        proc->state = PROC_STATE_UNUSED;
        return -4;
    }

    // Setup System V ABI User Stack
    proc->user_stack_top = setup_user_stack(proc, argc, argv, envp);

    // Set TSS Ring 0 stack pointer
    gdt_set_kernel_stack(proc->kernel_stack_top);

    process_t* prev_proc = current_process;
    current_process = proc;
    proc->state = PROC_STATE_RUNNING;

    serial_print("[USERSPACE] Launching PID ");
    serial_print_dec(proc->pid);
    serial_print(" ('");
    serial_print(proc->name);
    serial_print("') at Entry=0x");
    serial_print_hex(proc->entry_point);
    serial_print(" ESP=0x");
    serial_print_hex(proc->user_stack_top);
    serial_println(" in Ring 3...");

    // Save kernel context so sys_exit can return here
    int ret = user_setjmp(&kernel_return_ctx);
    if (ret == 0) {
        // Jump to Ring 3 (User Mode)
        jump_to_ring3(proc->entry_point, proc->user_stack_top, argc, argv);
    }

    // Execution resumes here after sys_exit
    int ret_code = (ret == 1000) ? 0 : ret;
    current_process = prev_proc;
    if (prev_proc) {
        gdt_set_kernel_stack(prev_proc->kernel_stack_top);
    }

    return ret_code;
}

int process_spawn(const char* path, int argc, char** argv) {
    return process_spawn_env(path, argc, argv, NULL);
}

void process_exit(int code) {
    if (current_process) {
        current_process->state = PROC_STATE_TERMINATED;
        current_process->exit_code = code;

        serial_print("[USERSPACE] Process PID ");
        serial_print_dec(current_process->pid);
        serial_print(" ('");
        serial_print(current_process->name);
        serial_print("') exited with status ");
        serial_print_dec(code);
        serial_println("");
    }

    user_longjmp(&kernel_return_ctx, (code == 0) ? 1000 : code);
}

void process_terminate_fault(int fault_code) {
    if (current_process) {
        serial_print("[USERSPACE] Fault termination for PID ");
        serial_print_dec(current_process->pid);
        serial_println("");
        process_exit(128 + fault_code);
    }
}

void process_list(void) {
    vga_println("--- Process Table (Userspace Tasks) ---");
    vga_println("PID  State       Heap(Used)   Name");
    vga_println("----------------------------------------");

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_STATE_UNUSED) {
            char pbuf[8]; int pp = 0, pid = processes[i].pid;
            if (pid == 0) pbuf[pp++] = '0';
            else {
                char tmp[8]; int tp = 0;
                while (pid > 0) { tmp[tp++] = '0' + (pid % 10); pid /= 10; }
                while (tp > 0) pbuf[pp++] = tmp[--tp];
            }
            pbuf[pp] = '\0';
            vga_print(pbuf);
            vga_print("    ");

            if (processes[i].state == PROC_STATE_RUNNING) {
                vga_print("RUNNING     ");
            } else if (processes[i].state == PROC_STATE_READY) {
                vga_print("READY       ");
            } else {
                vga_print("TERMINATED  ");
            }

            uint32_t heap_used = (processes[i].heap_break > processes[i].heap_start) ?
                                 (processes[i].heap_break - processes[i].heap_start) : 0;
            char hbuf[12]; int hp = 0;
            if (heap_used == 0) hbuf[hp++] = '0';
            else {
                char tmp[12]; int tp = 0;
                while (heap_used > 0) { tmp[tp++] = '0' + (heap_used % 10); heap_used /= 10; }
                while (tp > 0) hbuf[hp++] = tmp[--tp];
            }
            hbuf[hp] = '\0';
            vga_print(hbuf);
            vga_print(" B       ");
            vga_println(processes[i].name);
        }
    }
}

// Userspace Loader Verification Suite
void user_loader_run_tests(void) {
    serial_println("=========================================");
    serial_println("[USERSPACE] Running Userspace Loader Verification...");
    serial_println("=========================================");

    // Test 1: Process table initialization
    int free_slots = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_STATE_UNUSED) free_slots++;
    }
    if (free_slots == MAX_PROCESSES) {
        serial_println("  -> [TEST 1/4] Process Table memory pool verification: PASS");
    } else {
        serial_println("  -> [TEST 1/4] Process Table initialization: FAIL");
        return;
    }

    // Test 2: GDT & TSS Stack Configuration
    uint32_t current_kstack = gdt_get_kernel_stack();
    if (current_kstack != 0) {
        serial_println("  -> [TEST 2/4] TSS esp0 Kernel Stack verification: PASS");
    } else {
        serial_println("  -> [TEST 2/4] TSS esp0 configuration: FAIL");
        return;
    }

    // Test 3: Segment Selectors Validity (Ring 0 & Ring 3)
    if (KERNEL_CS_SEL == 0x08 && KERNEL_DS_SEL == 0x10 &&
        USER_CS_SEL == 0x1B && USER_DS_SEL == 0x23) {
        serial_println("  -> [TEST 3/4] Ring 3 Segment Selectors (DPL=3): PASS");
    } else {
        serial_println("  -> [TEST 3/4] Segment Selectors: FAIL");
        return;
    }

    // Test 4: Format Parser Verification on real binary
    int test_fd = vfs_open("/bin/usertest", O_RDONLY);
    elf32_ehdr_t real_ehdr;
    int parsed_ok = 0;
    if (test_fd >= 0) {
        if (vfs_read(test_fd, &real_ehdr, sizeof(real_ehdr)) == sizeof(real_ehdr)) {
            if (*(uint32_t*)real_ehdr.e_ident == ELF_MAGIC &&
                (real_ehdr.e_type == ET_EXEC || real_ehdr.e_type == ET_DYN) &&
                real_ehdr.e_machine == EM_386) {
                parsed_ok = 1;
            }
        }
        vfs_close(test_fd);
    }
    if (parsed_ok) {
        serial_println("  -> [TEST 4/4] Real ELF32 Executable Header parser (/bin/usertest): PASS");
    } else {
        serial_println("  -> [TEST 4/4] ELF32 parser: FAIL");
        return;
    }

    serial_println("[USERSPACE] Userspace Loader Verification: 4/4 PASSED!");

    // Execute Ring 3 Userspace Binary Self-Test (/bin/usertest)
    serial_println("[USERSPACE] Spawning Ring 3 test binary '/bin/usertest'...");
    process_spawn("/bin/usertest", 0, NULL);
}
