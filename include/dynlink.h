#ifndef DYNLINK_H
#define DYNLINK_H

#include <stdint.h>
#include <stddef.h>
#include "user_loader.h"

// ELF32 Dynamic Types & Flags
#define PT_DYNAMIC      2
#define PT_INTERP       3
#define PT_SHLIB        5
#define PT_PHDR         6
#define PT_TLS          7
#define PT_GNU_EH_FRAME 0x6474e550
#define PT_GNU_STACK    0x6474e551
#define PT_GNU_RELRO    0x6474e552

// Dynamic Array Tags (d_tag)
#define DT_NULL         0
#define DT_NEEDED       1
#define DT_PLTRELSZ     2
#define DT_PLTGOT       3
#define DT_HASH         4
#define DT_STRTAB       5
#define DT_SYMTAB       6
#define DT_RELA         7
#define DT_RELASZ       8
#define DT_RELAENT      9
#define DT_STRSZ        10
#define DT_SYMENT       11
#define DT_INIT         12
#define DT_FINI         13
#define DT_SONAME       14
#define DT_RPATH        15
#define DT_SYMBOLIC     16
#define DT_REL          17
#define DT_RELSZ        18
#define DT_RELENT       19
#define DT_PLTREL       20
#define DT_DEBUG        21
#define DT_TEXTREL      22
#define DT_JMPREL       23
#define DT_BIND_NOW     24
#define DT_INIT_ARRAY   25
#define DT_FINI_ARRAY   26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_RUNPATH      29
#define DT_FLAGS        30
#define DT_GNU_HASH     0x6ffffef5

typedef struct {
    int32_t  d_tag;
    union {
        uint32_t d_val;
        uint32_t d_ptr;
    } d_un;
} __attribute__((packed)) elf32_dyn_t;

// ELF Symbol Structure (Elf32_Sym)
typedef struct {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
} __attribute__((packed)) elf32_sym_t;

#define ELF32_ST_BIND(i)   ((i) >> 4)
#define ELF32_ST_TYPE(i)   ((i) & 0x0F)
#define ELF32_ST_INFO(b,t) (((b) << 4) + ((t) & 0x0F))

#define STB_LOCAL   0
#define STB_GLOBAL  1
#define STB_WEAK    2

#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4
#define STT_COMMON  5
#define STT_TLS     6

#define SHN_UNDEF   0
#define SHN_ABS     0xFFF1
#define SHN_COMMON  0xFFF2

// ELF Relocation Structures
typedef struct {
    uint32_t r_offset;
    uint32_t r_info;
} __attribute__((packed)) elf32_rel_t;

typedef struct {
    uint32_t r_offset;
    uint32_t r_info;
    int32_t  r_addend;
} __attribute__((packed)) elf32_rela_t;

#define ELF32_R_SYM(i)    ((i) >> 8)
#define ELF32_R_TYPE(i)   ((uint8_t)(i))
#define ELF32_R_INFO(s,t) (((s) << 8) + (uint8_t)(t))

// x86 Relocation Types
#define R_386_NONE      0
#define R_386_32        1
#define R_386_PC32      2
#define R_386_GOT32     3
#define R_386_PLT32     4
#define R_386_COPY      5
#define R_386_GLOB_DAT  6
#define R_386_JMP_SLOT  7
#define R_386_RELATIVE  8
#define R_386_GOTOFF    9
#define R_386_GOTPC     10

// Shared Object / Module Link Map
#define MAX_LOADED_MODULES  16
#define MAX_MODULE_NAME     64
#define MAX_NEEDED_LIBS     8

typedef struct so_module {
    char        name[MAX_MODULE_NAME];
    char        path[128];
    uint32_t    base_addr;          // Relocation base address (0 for ET_EXEC, load_base for ET_DYN)
    uint32_t    mem_size;
    int         is_executable;      // 1 if main executable, 0 if shared object (.so)
    uint32_t    entry_point;

    // Dynamic section pointers
    elf32_dyn_t* dynamic;
    uint32_t     dynamic_size;

    // Symbol & String Tables
    elf32_sym_t* symtab;
    uint32_t     symtab_count;
    const char*  strtab;
    uint32_t     strtab_size;

    // Standard SysV Hash table
    uint32_t*    hash;
    uint32_t     nbucket;
    uint32_t     nchain;
    uint32_t*    bucket;
    uint32_t*    chain;

    // Global Offset Table (GOT) & PLT
    uint32_t*    got;               // Points to _GLOBAL_OFFSET_TABLE_
    uint32_t     got_entries_count;

    // Relocations
    elf32_rel_t* rel;
    uint32_t     rel_size;
    elf32_rel_t* jmprel;            // PLT relocations (R_386_JMP_SLOT)
    uint32_t     jmprel_size;

    // Initialization & Termination
    void (*init_func)(void);
    void (*fini_func)(void);

    // Dependencies (DT_NEEDED)
    char         needed[MAX_NEEDED_LIBS][MAX_MODULE_NAME];
    int          needed_count;

    int          ref_count;
    int          loaded;
    int          relocated;

    struct so_module* next;
} so_module_t;

// Dynamic Linker Engine API
void         dynlink_init(void);
so_module_t* dynlink_load_library(const char* name);
int          dynlink_load_dependencies(so_module_t* mod);
int          dynlink_relocate_module(so_module_t* mod);
uint32_t     dynlink_resolve_symbol(so_module_t* requesting_mod, const char* name, elf32_sym_t** out_sym);
uint32_t     dynlink_lookup_global(const char* name);
int          dynlink_prepare_executable(process_t* proc, int fd, elf32_ehdr_t* ehdr);

// Lazy binding resolver (called by PLT[0] trampoline)
uint32_t     dynlink_runtime_resolve(uint32_t mod_id, uint32_t reloc_offset);

// Standard ELF hashing function
uint32_t     elf_hash(const char* name);

// Diagnostic & Inspection API
void         dynlink_dump_module(const char* path);
void         dynlink_ldd(const char* path);
void         dynlink_dump_got(const char* path);
void         dynlink_dump_plt(const char* path);
void         dynlink_run_tests(void);

#endif // DYNLINK_H
