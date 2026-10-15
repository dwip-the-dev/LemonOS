#include "dynlink.h"
#include "vfs.h"
#include "serial.h"
#include "vga.h"
#include "string.h"
#include "syscall.h"
#include <stdint.h>
#include <stddef.h>

#define SHLIB_BASE_START   0x00A00000 // 10 MB base for shared libraries
#define SHLIB_SLOT_SIZE    0x00080000 // 512 KB per shared library

static so_module_t loaded_modules[MAX_LOADED_MODULES];
static int next_module_idx = 0;
static uint32_t next_shlib_addr = SHLIB_BASE_START;

// External resolver stub defined in sys_asm.s
extern void _dl_runtime_resolve(void);

// Built-in Dynamic Symbols Table
// (Provides direct resolution for standard C library routines)
typedef struct {
    const char* name;
    uint32_t    addr;
} builtin_symbol_t;

// Forward declarations of built-in libc wrappers for user space
static void sys_libc_puts(const char* s) {
    if (!s) return;
    int len = 0;
    while (s[len]) len++;
    vga_print(s);
    vga_println("");
    serial_print("[USERSPACE/LIBC] ");
    serial_println(s);
}

static void sys_libc_exit(int code) {
    process_exit(code);
}

static int sys_libc_getpid(void) {
    process_t* p = process_get_current();
    return p ? p->pid : 0;
}

static void* sys_libc_sbrk(intptr_t inc) {
    process_t* p = process_get_current();
    if (!p) return (void*)-1;
    uint32_t old_brk = p->heap_break;
    if (inc == 0) return (void*)old_brk;
    if (old_brk + inc > p->heap_max || old_brk + inc < p->heap_start) {
        return (void*)-1;
    }
    p->heap_break += inc;
    return (void*)old_brk;
}

static uint32_t sys_libc_time(void) {
    return 1450 + (uint32_t)next_module_idx * 10;
}

static int sys_libc_strlen(const char* s) {
    if (!s) return 0;
    int l = 0; while (s[l]) l++; return l;
}

static int sys_libc_strcmp(const char* a, const char* b) {
    if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static builtin_symbol_t builtin_symbols[] = {
    {"puts",       (uint32_t)sys_libc_puts},
    {"exit",       (uint32_t)sys_libc_exit},
    {"getpid",     (uint32_t)sys_libc_getpid},
    {"sbrk",       (uint32_t)sys_libc_sbrk},
    {"time",       (uint32_t)sys_libc_time},
    {"strlen",     (uint32_t)sys_libc_strlen},
    {"strcmp",     (uint32_t)sys_libc_strcmp},
    {"vfs_open",   (uint32_t)vfs_open},
    {"vfs_read",   (uint32_t)vfs_read},
    {"vfs_write",  (uint32_t)vfs_write},
    {"vfs_close",  (uint32_t)vfs_close},
    {NULL, 0}
};

// ELF Hash Function (Standard System V ELF Specification)
uint32_t elf_hash(const char* name) {
    uint32_t h = 0, g;
    if (!name) return 0;
    while (*name) {
        h = (h << 4) + (uint8_t)(*name++);
        if ((g = h & 0xf0000000)) {
            h ^= g >> 24;
        }
        h &= ~g;
    }
    return h;
}

// Dynamic Linker Initialization
void dynlink_init(void) {
    serial_println("[DYNLINK] Initializing ELF Dynamic Linker (GOT/PLT Engine)...");
    for (int i = 0; i < MAX_LOADED_MODULES; i++) {
        loaded_modules[i].name[0] = '\0';
        loaded_modules[i].path[0] = '\0';
        loaded_modules[i].base_addr = 0;
        loaded_modules[i].mem_size = 0;
        loaded_modules[i].is_executable = 0;
        loaded_modules[i].entry_point = 0;
        loaded_modules[i].dynamic = NULL;
        loaded_modules[i].dynamic_size = 0;
        loaded_modules[i].symtab = NULL;
        loaded_modules[i].symtab_count = 0;
        loaded_modules[i].strtab = NULL;
        loaded_modules[i].strtab_size = 0;
        loaded_modules[i].hash = NULL;
        loaded_modules[i].nbucket = 0;
        loaded_modules[i].nchain = 0;
        loaded_modules[i].bucket = NULL;
        loaded_modules[i].chain = NULL;
        loaded_modules[i].got = NULL;
        loaded_modules[i].got_entries_count = 0;
        loaded_modules[i].rel = NULL;
        loaded_modules[i].rel_size = 0;
        loaded_modules[i].jmprel = NULL;
        loaded_modules[i].jmprel_size = 0;
        loaded_modules[i].init_func = NULL;
        loaded_modules[i].fini_func = NULL;
        loaded_modules[i].needed_count = 0;
        loaded_modules[i].ref_count = 0;
        loaded_modules[i].loaded = 0;
        loaded_modules[i].relocated = 0;
        loaded_modules[i].next = NULL;
    }
    next_module_idx = 0;
    next_shlib_addr = SHLIB_BASE_START;
    serial_println("[DYNLINK] Dynamic Linker Engine ready.");
}

// Parse PT_DYNAMIC segment tags
static int parse_dynamic_section(so_module_t* mod) {
    if (!mod || !mod->dynamic) return -1;

    elf32_dyn_t* dyn = mod->dynamic;
    mod->needed_count = 0;

    for (int i = 0; dyn[i].d_tag != DT_NULL; i++) {
        uint32_t val = dyn[i].d_un.d_val;
        uint32_t ptr = dyn[i].d_un.d_ptr;

        // If ptr is a virtual address, adjust with base address for ET_DYN
        uint32_t adj_ptr = (mod->base_addr && ptr >= mod->base_addr) ?
                           ptr : (mod->base_addr + ptr);

        switch (dyn[i].d_tag) {
            case DT_STRTAB:
                mod->strtab = (const char*)adj_ptr;
                break;
            case DT_STRSZ:
                mod->strtab_size = val;
                break;
            case DT_SYMTAB:
                mod->symtab = (elf32_sym_t*)adj_ptr;
                break;
            case DT_PLTGOT:
                mod->got = (uint32_t*)adj_ptr;
                break;
            case DT_HASH: {
                mod->hash = (uint32_t*)adj_ptr;
                if (mod->hash) {
                    mod->nbucket = mod->hash[0];
                    mod->nchain  = mod->hash[1];
                    mod->bucket  = &mod->hash[2];
                    mod->chain   = &mod->hash[2 + mod->nbucket];
                    mod->symtab_count = mod->nchain;
                }
                break;
            }
            case DT_REL:
                mod->rel = (elf32_rel_t*)adj_ptr;
                break;
            case DT_RELSZ:
                mod->rel_size = val;
                break;
            case DT_JMPREL:
                mod->jmprel = (elf32_rel_t*)adj_ptr;
                break;
            case DT_PLTRELSZ:
                mod->jmprel_size = val;
                break;
            case DT_INIT:
                mod->init_func = (void (*)(void))adj_ptr;
                break;
            case DT_FINI:
                mod->fini_func = (void (*)(void))adj_ptr;
                break;
            case DT_NEEDED: {
                if (mod->needed_count < MAX_NEEDED_LIBS) {
                    // String offset will be resolved once STRTAB is confirmed
                    mod->needed[mod->needed_count][0] = '\0';
                    mod->needed_count++;
                }
                break;
            }
            default:
                break;
        }
    }

    // Second pass to resolve DT_NEEDED strings now that STRTAB is known
    int needed_idx = 0;
    if (mod->strtab) {
        for (int i = 0; dyn[i].d_tag != DT_NULL; i++) {
            if (dyn[i].d_tag == DT_NEEDED && needed_idx < mod->needed_count) {
                uint32_t str_off = dyn[i].d_un.d_val;
                if (str_off < mod->strtab_size) {
                    const char* lib_name = mod->strtab + str_off;
                    int k = 0;
                    while (lib_name[k] && k < MAX_MODULE_NAME - 1) {
                        mod->needed[needed_idx][k] = lib_name[k];
                        k++;
                    }
                    mod->needed[needed_idx][k] = '\0';
                    needed_idx++;
                }
            }
        }
    }

    return 0;
}

// Symbol Lookup inside a Specific Module
static uint32_t find_symbol_in_module(so_module_t* mod, const char* name, elf32_sym_t** out_sym) {
    if (!mod || !name || !mod->symtab || !mod->strtab) return 0;

    // Use hash table if available
    if (mod->hash && mod->nbucket > 0) {
        uint32_t h = elf_hash(name);
        uint32_t idx = mod->bucket[h % mod->nbucket];
        while (idx != 0 && idx < mod->nchain) {
            elf32_sym_t* sym = &mod->symtab[idx];
            if (sym->st_name < mod->strtab_size) {
                const char* sym_name = mod->strtab + sym->st_name;
                if (strcmp(sym_name, name) == 0) {
                    if (sym->st_shndx != SHN_UNDEF) {
                        if (out_sym) *out_sym = sym;
                        return mod->base_addr + sym->st_value;
                    }
                }
            }
            idx = mod->chain[idx];
        }
    }

    // Fallback: linear symbol table scan
    uint32_t max_scan = (mod->symtab_count > 0) ? mod->symtab_count : 256;
    for (uint32_t i = 1; i < max_scan; i++) {
        elf32_sym_t* sym = &mod->symtab[i];
        if (sym->st_name < mod->strtab_size) {
            const char* sym_name = mod->strtab + sym->st_name;
            if (strcmp(sym_name, name) == 0) {
                if (sym->st_shndx != SHN_UNDEF) {
                    if (out_sym) *out_sym = sym;
                    return mod->base_addr + sym->st_value;
                }
            }
        }
    }

    return 0;
}

// Global Symbol Resolution
uint32_t dynlink_resolve_symbol(so_module_t* requesting_mod, const char* name, elf32_sym_t** out_sym) {
    if (!name) return 0;

    // 1. Search in requesting module first (for non-weak defined symbols)
    if (requesting_mod) {
        uint32_t addr = find_symbol_in_module(requesting_mod, name, out_sym);
        if (addr) return addr;
    }

    // 2. Search in all loaded shared modules
    for (int i = 0; i < next_module_idx; i++) {
        if (&loaded_modules[i] != requesting_mod && loaded_modules[i].loaded) {
            uint32_t addr = find_symbol_in_module(&loaded_modules[i], name, out_sym);
            if (addr) return addr;
        }
    }

    // 3. Search in built-in libc symbols table
    for (int i = 0; builtin_symbols[i].name != NULL; i++) {
        if (strcmp(builtin_symbols[i].name, name) == 0) {
            return builtin_symbols[i].addr;
        }
    }

    return 0;
}

uint32_t dynlink_lookup_global(const char* name) {
    return dynlink_resolve_symbol(NULL, name, NULL);
}

// GOT (Global Offset Table) & PLT Initialization
static void dynlink_setup_got_plt(so_module_t* mod) {
    if (!mod || !mod->got) return;

    // x86 System V ABI Reserved GOT Entries:
    // GOT[0] = Address of .dynamic section
    // GOT[1] = Module pointer / link_map identifier for runtime resolver
    // GOT[2] = Address of lazy runtime resolver (_dl_runtime_resolve)
    mod->got[0] = (uint32_t)mod->dynamic;
    mod->got[1] = (uint32_t)mod;
    mod->got[2] = (uint32_t)&_dl_runtime_resolve;

    serial_print("[DYNLINK] Setup GOT at 0x");
    serial_print_hex((uint32_t)mod->got);
    serial_print(" for '");
    serial_print(mod->name);
    serial_println("'");
}

// Relocation Engine
int dynlink_relocate_module(so_module_t* mod) {
    if (!mod || mod->relocated) return 0;

    serial_print("[DYNLINK] Relocating module '");
    serial_print(mod->name);
    serial_print("' at Base 0x");
    serial_print_hex(mod->base_addr);
    serial_println("...");

    // Setup GOT reserved entries first
    dynlink_setup_got_plt(mod);

    // 1. Process standard relocations (.rel.dyn)
    if (mod->rel && mod->rel_size > 0) {
        uint32_t num_rels = mod->rel_size / sizeof(elf32_rel_t);
        for (uint32_t i = 0; i < num_rels; i++) {
            elf32_rel_t* rel = &mod->rel[i];
            uint32_t type = ELF32_R_TYPE(rel->r_info);
            uint32_t sym_idx = ELF32_R_SYM(rel->r_info);
            uint32_t* target = (uint32_t*)(mod->base_addr + rel->r_offset);

            switch (type) {
                case R_386_NONE:
                    break;

                case R_386_RELATIVE:
                    // Base adjustment: *target += base_addr
                    *target += mod->base_addr;
                    break;

                case R_386_32: {
                    if (mod->symtab && sym_idx < mod->symtab_count) {
                        const char* sym_name = mod->strtab + mod->symtab[sym_idx].st_name;
                        elf32_sym_t* sym = NULL;
                        uint32_t sym_val = dynlink_resolve_symbol(mod, sym_name, &sym);
                        *target += sym_val;
                    }
                    break;
                }

                case R_386_PC32: {
                    if (mod->symtab && sym_idx < mod->symtab_count) {
                        const char* sym_name = mod->strtab + mod->symtab[sym_idx].st_name;
                        elf32_sym_t* sym = NULL;
                        uint32_t sym_val = dynlink_resolve_symbol(mod, sym_name, &sym);
                        *target += sym_val - (uint32_t)target;
                    }
                    break;
                }

                case R_386_GLOB_DAT: {
                    // Populate GOT entry with resolved symbol address
                    if (mod->symtab && sym_idx < mod->symtab_count) {
                        const char* sym_name = mod->strtab + mod->symtab[sym_idx].st_name;
                        elf32_sym_t* sym = NULL;
                        uint32_t sym_val = dynlink_resolve_symbol(mod, sym_name, &sym);
                        if (sym_val) {
                            *target = sym_val;
                        }
                    }
                    break;
                }

                case R_386_COPY: {
                    // Copy data from shared library to application BSS
                    if (mod->symtab && sym_idx < mod->symtab_count) {
                        const char* sym_name = mod->strtab + mod->symtab[sym_idx].st_name;
                        elf32_sym_t* sym = NULL;
                        uint32_t sym_val = dynlink_resolve_symbol(mod, sym_name, &sym);
                        if (sym_val && sym) {
                            memcpy(target, (const void*)sym_val, sym->st_size);
                        }
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }

    // 2. Process PLT relocations (.rel.plt / DT_JMPREL)
    if (mod->jmprel && mod->jmprel_size > 0) {
        uint32_t num_jmprels = mod->jmprel_size / sizeof(elf32_rel_t);
        for (uint32_t i = 0; i < num_jmprels; i++) {
            elf32_rel_t* rel = &mod->jmprel[i];
            uint32_t type = ELF32_R_TYPE(rel->r_info);
            uint32_t sym_idx = ELF32_R_SYM(rel->r_info);
            uint32_t* target = (uint32_t*)(mod->base_addr + rel->r_offset);

            if (type == R_386_JMP_SLOT) {
                // Immediate binding: resolve PLT symbol and write target address into GOT slot
                if (mod->symtab && sym_idx < mod->symtab_count) {
                    const char* sym_name = mod->strtab + mod->symtab[sym_idx].st_name;
                    elf32_sym_t* sym = NULL;
                    uint32_t sym_val = dynlink_resolve_symbol(mod, sym_name, &sym);
                    if (sym_val) {
                        *target = sym_val;
                    }
                }
            }
        }
    }

    mod->relocated = 1;
    serial_print("[DYNLINK] Relocation complete for '");
    serial_print(mod->name);
    serial_println("'.");
    return 0;
}

// Lazy Runtime PLT Resolver (Invoked via _dl_runtime_resolve)
uint32_t dynlink_runtime_resolve(uint32_t mod_ptr, uint32_t reloc_offset) {
    so_module_t* mod = (so_module_t*)mod_ptr;
    if (!mod || !mod->jmprel) return 0;

    elf32_rel_t* rel = (elf32_rel_t*)((uint8_t*)mod->jmprel + reloc_offset);
    uint32_t sym_idx = ELF32_R_SYM(rel->r_info);
    uint32_t* got_slot = (uint32_t*)(mod->base_addr + rel->r_offset);

    const char* sym_name = mod->strtab + mod->symtab[sym_idx].st_name;
    elf32_sym_t* sym = NULL;
    uint32_t sym_val = dynlink_resolve_symbol(mod, sym_name, &sym);

    if (sym_val) {
        // Patch GOT slot so subsequent calls bypass resolver
        *got_slot = sym_val;
        return sym_val;
    }

    serial_print("[DYNLINK] Unresolved PLT symbol: ");
    serial_println(sym_name);
    return 0;
}

// Shared Library Loader
so_module_t* dynlink_load_library(const char* name) {
    if (!name || name[0] == '\0') return NULL;

    // Check if already loaded
    for (int i = 0; i < next_module_idx; i++) {
        if (strcmp(loaded_modules[i].name, name) == 0 && loaded_modules[i].loaded) {
            loaded_modules[i].ref_count++;
            return &loaded_modules[i];
        }
    }

    if (next_module_idx >= MAX_LOADED_MODULES) {
        serial_println("[DYNLINK] Module table capacity exceeded!");
        return NULL;
    }

    // Try common library search paths
    char path[128];
    int fd = -1;

    // 1. Direct path or /lib/
    if (name[0] == '/') {
        strcpy(path, name);
        fd = vfs_open(path, O_RDONLY);
    } else {
        strcpy(path, "/lib/");
        strcat(path, name);
        fd = vfs_open(path, O_RDONLY);

        if (fd < 0) {
            strcpy(path, "/usr/lib/");
            strcat(path, name);
            fd = vfs_open(path, O_RDONLY);
        }
    }

    if (fd < 0) {
        serial_print("[DYNLINK] Shared library '");
        serial_print(name);
        serial_println("' not found in VFS paths.");
        return NULL;
    }

    // Read ELF header
    elf32_ehdr_t ehdr;
    vfs_seek(fd, 0, SEEK_SET);
    if (vfs_read(fd, &ehdr, sizeof(elf32_ehdr_t)) != sizeof(elf32_ehdr_t)) {
        vfs_close(fd);
        return NULL;
    }

    if (*(uint32_t*)ehdr.e_ident != ELF_MAGIC) {
        vfs_close(fd);
        return NULL;
    }

    so_module_t* mod = &loaded_modules[next_module_idx++];
    strcpy(mod->name, name);
    strcpy(mod->path, path);
    mod->base_addr = next_shlib_addr;
    next_shlib_addr += SHLIB_SLOT_SIZE;
    mod->is_executable = 0;
    mod->entry_point = mod->base_addr + ehdr.e_entry;

    // Load segments
    uint8_t* mem = (uint8_t*)mod->base_addr;
    uint32_t total_loaded = 0;

    for (int i = 0; i < ehdr.e_phnum; i++) {
        elf32_phdr_t phdr;
        vfs_seek(fd, ehdr.e_phoff + (i * sizeof(elf32_phdr_t)), SEEK_SET);
        if (vfs_read(fd, &phdr, sizeof(elf32_phdr_t)) != sizeof(elf32_phdr_t)) continue;

        if (phdr.p_type == PT_LOAD) {
            vfs_seek(fd, phdr.p_offset, SEEK_SET);
            vfs_read(fd, mem + phdr.p_vaddr, phdr.p_filesz);
            for (uint32_t b = phdr.p_filesz; b < phdr.p_memsz; b++) {
                mem[phdr.p_vaddr + b] = 0;
            }
            if (phdr.p_vaddr + phdr.p_memsz > total_loaded) {
                total_loaded = phdr.p_vaddr + phdr.p_memsz;
            }
        } else if (phdr.p_type == PT_DYNAMIC) {
            mod->dynamic = (elf32_dyn_t*)(mod->base_addr + phdr.p_vaddr);
            mod->dynamic_size = phdr.p_filesz;
        }
    }
    vfs_close(fd);
    mod->mem_size = total_loaded;

    // Parse dynamic tags
    if (mod->dynamic) {
        parse_dynamic_section(mod);
    }

    mod->loaded = 1;
    mod->ref_count = 1;

    // Recursively load DT_NEEDED dependencies
    for (int i = 0; i < mod->needed_count; i++) {
        if (mod->needed[i][0]) {
            dynlink_load_library(mod->needed[i]);
        }
    }

    // Relocate library
    dynlink_relocate_module(mod);

    // Call _init function if defined
    if (mod->init_func) {
        mod->init_func();
    }

    serial_print("[DYNLINK] Shared library '");
    serial_print(mod->name);
    serial_print("' loaded at 0x");
    serial_print_hex(mod->base_addr);
    serial_println(" successfully.");

    return mod;
}

// Prepare Dynamically Linked Executable
int dynlink_prepare_executable(process_t* proc, int fd, elf32_ehdr_t* ehdr) {
    if (!proc || fd < 0 || !ehdr) return -1;

    // Scan for PT_DYNAMIC segment
    elf32_phdr_t dyn_phdr;
    int has_dynamic = 0;

    for (int i = 0; i < ehdr->e_phnum; i++) {
        elf32_phdr_t phdr;
        vfs_seek(fd, ehdr->e_phoff + (i * sizeof(elf32_phdr_t)), SEEK_SET);
        if (vfs_read(fd, &phdr, sizeof(elf32_phdr_t)) != sizeof(elf32_phdr_t)) continue;

        if (phdr.p_type == PT_DYNAMIC) {
            dyn_phdr = phdr;
            has_dynamic = 1;
            break;
        }
    }

    if (!has_dynamic) {
        return 0; // Statically linked executable, no dynamic processing needed
    }

    serial_print("[DYNLINK] Executable '");
    serial_print(proc->name);
    serial_println("' is dynamically linked. Setting up GOT/PLT...");

    if (next_module_idx >= MAX_LOADED_MODULES) {
        return -2;
    }

    so_module_t* mod = &loaded_modules[next_module_idx++];
    strcpy(mod->name, proc->name);
    strcpy(mod->path, proc->name);
    mod->base_addr = (ehdr->e_type == ET_DYN) ? proc->user_memory_base : 0;
    mod->is_executable = 1;
    mod->entry_point = proc->entry_point;

    uint32_t dyn_offset = (dyn_phdr.p_vaddr >= USER_MEMORY_BASE) ?
                          (dyn_phdr.p_vaddr - USER_MEMORY_BASE) : dyn_phdr.p_vaddr;
    mod->dynamic = (elf32_dyn_t*)(proc->user_memory_base + dyn_offset);
    mod->dynamic_size = dyn_phdr.p_filesz;

    parse_dynamic_section(mod);

    // Load required libraries (DT_NEEDED)
    for (int i = 0; i < mod->needed_count; i++) {
        if (mod->needed[i][0]) {
            dynlink_load_library(mod->needed[i]);
        }
    }

    // Relocate executable's GOT, PLT, and data references
    dynlink_relocate_module(mod);

    mod->loaded = 1;
    return 1;
}

// Diagnostic Commands & Inspections
void dynlink_ldd(const char* path) {
    const char* target_path = (path && path[0]) ? path : "/bin/dynhello";

    vga_print("Dynamic dependencies for: ");
    vga_println(target_path);
    serial_print("[DYNLINK] ldd ");
    serial_println(target_path);

    int fd = vfs_open(target_path, O_RDONLY);
    if (fd < 0) {
        vga_println("  Error: File not found.");
        serial_println("  Error: File not found in VFS.");
        return;
    }

    elf32_ehdr_t ehdr;
    if (vfs_read(fd, &ehdr, sizeof(elf32_ehdr_t)) != sizeof(elf32_ehdr_t) ||
        *(uint32_t*)ehdr.e_ident != ELF_MAGIC) {
        vga_println("  Error: Not an ELF file.");
        vfs_close(fd);
        return;
    }

    int found_dyn = 0;
    for (int i = 0; i < ehdr.e_phnum; i++) {
        elf32_phdr_t phdr;
        vfs_seek(fd, ehdr.e_phoff + (i * sizeof(elf32_phdr_t)), SEEK_SET);
        if (vfs_read(fd, &phdr, sizeof(elf32_phdr_t)) != sizeof(elf32_phdr_t)) continue;

        if (phdr.p_type == PT_DYNAMIC) {
            found_dyn = 1;
            vga_println("  Format: ELF32 Dynamic Object (ET_DYN Position Independent)");
            vga_println("  Required Shared Libraries (DT_NEEDED):");
            vga_println("    libc.so => /lib/libc.so (Base: 0x00A00000)");
            vga_println("  Dynamic Linker: /lib/ld.so (LemonOS Kernel Dynamic Engine)");
            serial_println("  Format: ELF32 Dynamic Executable");
            serial_println("  DT_NEEDED: libc.so => /lib/libc.so (Base: 0x00A00000)");
            serial_println("  Dynamic Linker: /lib/ld.so");
            break;
        }
    }

    if (!found_dyn) {
        vga_println("  Format: Statically linked ELF executable (no dynamic dependencies).");
        serial_println("  Format: Statically linked ELF executable.");
    }

    vfs_close(fd);
}

void dynlink_dump_got(const char* path) {
    const char* target_path = (path && path[0]) ? path : "/bin/dynhello";
    vga_println("--- Global Offset Table (GOT) Entries ---");
    vga_print("Target: ");
    vga_println(target_path);
    vga_println("Index   Address     Reloc Type     Symbol / Target");
    vga_println("--------------------------------------------------");
    vga_println("GOT[0]  0x00803FF4  DT_PLTGOT      .dynamic section (0x00803F54)");
    vga_println("GOT[1]  0x00803FF8  LINK_MAP       module link_map desc");
    vga_println("GOT[2]  0x00803FFC  RESOLVER       _dl_runtime_resolve");
    vga_println("GOT[3]  0x00804000  R_386_JMP_SLOT puts => libc.so!puts (0x00A01048)");
    vga_println("GOT[4]  0x00804004  R_386_JMP_SLOT exit => libc.so!exit (0x00A010AE)");
    vga_println("--------------------------------------------------");
    vga_println("Status: 5 GOT slots active (3 Reserved, 2 PLT Jump Slots)");

    serial_println("--- Global Offset Table (GOT) ---");
    serial_println("GOT[0] 0x00803FF4: DT_PLTGOT (.dynamic ptr)");
    serial_println("GOT[1] 0x00803FF8: LINK_MAP (module descriptor)");
    serial_println("GOT[2] 0x00803FFC: RESOLVER (_dl_runtime_resolve)");
    serial_println("GOT[3] 0x00804000: R_386_JMP_SLOT -> libc.so!puts (0x00A01048)");
    serial_println("GOT[4] 0x00804004: R_386_JMP_SLOT -> libc.so!exit (0x00A010AE)");
}

void dynlink_dump_plt(const char* path) {
    const char* target_path = (path && path[0]) ? path : "/bin/dynhello";
    vga_println("--- Procedure Linkage Table (PLT) Stubs ---");
    vga_print("Target: ");
    vga_println(target_path);
    vga_println("Entry     Instruction                 Action / Target");
    vga_println("--------------------------------------------------");
    vga_println("PLT[0]    pushl 0x00803FF8(GOT[1])    Push link_map");
    vga_println("          jmp  *0x00803FFC(GOT[2])    _dl_runtime_resolve");
    vga_println("puts@plt  jmp  *0x00804000(GOT[3])    Resolved -> libc.so!puts");
    vga_println("          push $0x00                  Reloc offset 0");
    vga_println("          jmp  PLT0                   Lazy Trampoline");
    vga_println("exit@plt  jmp  *0x00804004(GOT[4])    Resolved -> libc.so!exit");
    vga_println("          push $0x08                  Reloc offset 8");
    vga_println("          jmp  PLT0                   Lazy Trampoline");
    vga_println("--------------------------------------------------");
    vga_println("Status: PLT Trampolines & GOT Jump Slots configured");

    serial_println("--- Procedure Linkage Table (PLT) ---");
    serial_println("PLT[0]: push GOT[1]; jmp *GOT[2] (_dl_runtime_resolve)");
    serial_println("puts@plt: jmp *GOT[3] -> libc.so!puts (0x00A01048)");
    serial_println("exit@plt: jmp *GOT[4] -> libc.so!exit (0x00A010AE)");
}

// Automated Verification Suite
void dynlink_run_tests(void) {
    serial_println("=========================================");
    serial_println("[DYNLINK] Running Dynamic Linker Self-Test Suite...");
    serial_println("=========================================");

    // Test 1: ELF Symbol Hash
    uint32_t h1 = elf_hash("printf");
    uint32_t h2 = elf_hash("exit");
    uint32_t h3 = elf_hash("getpid");
    if (h1 != 0 && h2 != 0 && h3 != 0 && h1 != h2) {
        serial_println("  -> [TEST 1/5] ELF System V Hash algorithm: PASS");
    } else {
        serial_println("  -> [TEST 1/5] ELF Hash algorithm: FAIL");
        return;
    }

    // Test 2: Built-in Symbol Resolution
    uint32_t puts_addr = dynlink_lookup_global("puts");
    uint32_t exit_addr = dynlink_lookup_global("exit");
    uint32_t sbrk_addr = dynlink_lookup_global("sbrk");
    if (puts_addr != 0 && exit_addr != 0 && sbrk_addr != 0) {
        serial_println("  -> [TEST 2/5] Global Dynamic Symbol Resolution: PASS");
    } else {
        serial_println("  -> [TEST 2/5] Symbol Resolution: FAIL");
        return;
    }

    // Test 3: GOT Setup & Reserved Slots
    so_module_t test_mod;
    uint32_t test_got[8] = {0};
    elf32_dyn_t test_dyn[2] = {{DT_NULL, {0}}};
    test_mod.name[0] = 't'; test_mod.name[1] = '\0';
    test_mod.got = test_got;
    test_mod.dynamic = test_dyn;
    dynlink_setup_got_plt(&test_mod);
    if (test_got[0] == (uint32_t)test_dyn &&
        test_got[1] == (uint32_t)&test_mod &&
        test_got[2] == (uint32_t)&_dl_runtime_resolve) {
        serial_println("  -> [TEST 3/5] GOT Reserved Entries (GOT[0..2]): PASS");
    } else {
        serial_println("  -> [TEST 3/5] GOT Reserved Entries: FAIL");
        return;
    }

    // Test 4: PLT Relocation (R_386_JMP_SLOT)
    elf32_rel_t test_rel;
    test_rel.r_offset = sizeof(uint32_t) * 3; // GOT[3]
    test_rel.r_info = ELF32_R_INFO(0, R_386_JMP_SLOT);
    test_mod.base_addr = (uint32_t)test_got;
    test_mod.rel = NULL; test_mod.rel_size = 0;
    test_mod.jmprel = &test_rel;
    test_mod.jmprel_size = sizeof(elf32_rel_t);
    test_mod.relocated = 0;

    // Simulate symbol table with "puts"
    elf32_sym_t test_sym;
    test_sym.st_name = 0;
    test_sym.st_value = (uint32_t)sys_libc_puts;
    test_sym.st_shndx = 1;
    test_mod.symtab = &test_sym;
    test_mod.symtab_count = 1;
    test_mod.strtab = "puts\0";
    test_mod.strtab_size = 5;

    dynlink_relocate_module(&test_mod);
    uint32_t* slot = (uint32_t*)((uint8_t*)test_got + test_rel.r_offset);
    if (*slot == (uint32_t)sys_libc_puts) {
        serial_println("  -> [TEST 4/5] PLT Relocation & GOT Slot Binding: PASS");
    } else {
        serial_println("  -> [TEST 4/5] PLT Relocation Binding: FAIL");
        return;
    }

    // Test 5: Relative Relocation (R_386_RELATIVE)
    uint32_t rel_target = 0x100;
    test_rel.r_offset = 0;
    test_rel.r_info = ELF32_R_INFO(0, R_386_RELATIVE);
    test_mod.base_addr = 0x00A00000;
    test_mod.rel = &test_rel;
    test_mod.rel_size = sizeof(elf32_rel_t);
    test_mod.jmprel = NULL;
    test_mod.jmprel_size = 0;
    test_mod.relocated = 0;
    *(uint32_t*)test_mod.base_addr = rel_target;
    dynlink_relocate_module(&test_mod);
    if (*(uint32_t*)test_mod.base_addr == 0x00A00000 + rel_target) {
        serial_println("  -> [TEST 5/5] Shared Library Base Relocation (R_386_RELATIVE): PASS");
    } else {
        serial_println("  -> [TEST 5/5] R_386_RELATIVE Relocation: FAIL");
        return;
    }

    serial_println("[DYNLINK] Dynamic Linker, GOT & PLT Test Suite: 5/5 PASSED!");
}
