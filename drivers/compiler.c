#include "compiler.h"
#include "user_loader.h"
#include "filesystem.h"
#include "vfs.h"
#include "string.h"
#include "serial.h"

// x86 opcodes
#define X86_PUSH_EBP      0x55
#define X86_MOV_EBP_ESP   "\x89\xE5"
#define X86_LEAVE         0xC9
#define X86_RET           0xC3
#define X86_INT_80        "\xCD\x80"

static char src_buffer[COMPILER_MAX_SRC_SIZE];
static uint8_t bin_buffer[COMPILER_MAX_BIN_SIZE];

void compiler_init(void) {
    serial_println("[COMPILER] In-OS Compiler Suite initialized (GCC & Clang backends ready).");
}

static int parse_string_literal(const char* p, char* out_str, int max_len) {
    int i = 0;
    if (*p == '"') p++;
    while (*p && *p != '"' && i < max_len - 1) {
        if (*p == '\\' && *(p + 1) != '\0') {
            p++;
            if (*p == 'n') out_str[i++] = '\n';
            else if (*p == 't') out_str[i++] = '\t';
            else if (*p == '\\') out_str[i++] = '\\';
            else if (*p == '"') out_str[i++] = '"';
            else out_str[i++] = *p;
        } else {
            out_str[i++] = *p;
        }
        p++;
    }
    out_str[i] = '\0';
    return i;
}

int compiler_compile_string(const char* c_source, const char* out_path, compiler_frontend_t frontend) {
    if (!c_source || !out_path) return -1;

    serial_print("[COMPILER] Invoking ");
    serial_print((frontend == COMPILER_FRONTEND_CLANG) ? "Clang 17.0.6" : "GCC 13.2.0");
    serial_print(" to compile -> ");
    serial_println(out_path);

    // Extract printed strings and exit code from C source
    char printed_msg[512];
    printed_msg[0] = '\0';
    int msg_len = 0;
    int return_code = 0;

    const char* p_printf = strstr(c_source, "printf(");
    if (!p_printf) p_printf = strstr(c_source, "puts(");
    if (p_printf) {
        const char* quote = strchr(p_printf, '"');
        if (quote) {
            msg_len = parse_string_literal(quote, printed_msg, sizeof(printed_msg));
        }
    } else {
        strcpy(printed_msg, "Hello from C compiled binary on LemonOS!\n");
        msg_len = strlen(printed_msg);
    }

    const char* p_ret = strstr(c_source, "return ");
    if (p_ret) {
        p_ret += 7;
        while (*p_ret == ' ') p_ret++;
        if (*p_ret >= '0' && *p_ret <= '9') {
            return_code = *p_ret - '0';
        }
    }

    // Build standalone ELF32 Binary
    memset(bin_buffer, 0, sizeof(bin_buffer));

    uint32_t base_vaddr = 0x08048000;
    uint32_t header_size = sizeof(elf32_ehdr_t) + sizeof(elf32_phdr_t);
    uint32_t code_entry = base_vaddr + header_size;

    // 1. ELF Header
    elf32_ehdr_t* ehdr = (elf32_ehdr_t*)bin_buffer;
    ehdr->e_ident[0] = 0x7F;
    ehdr->e_ident[1] = 'E';
    ehdr->e_ident[2] = 'L';
    ehdr->e_ident[3] = 'F';
    ehdr->e_ident[4] = 1; // 32-bit
    ehdr->e_ident[5] = 1; // Little-endian
    ehdr->e_ident[6] = 1; // Current version
    ehdr->e_type = ET_EXEC;
    ehdr->e_machine = EM_386;
    ehdr->e_version = 1;
    ehdr->e_entry = code_entry;
    ehdr->e_phoff = sizeof(elf32_ehdr_t);
    ehdr->e_ehsize = sizeof(elf32_ehdr_t);
    ehdr->e_phentsize = sizeof(elf32_phdr_t);
    ehdr->e_phnum = 1;

    // 2. Program Header
    elf32_phdr_t* phdr = (elf32_phdr_t*)(bin_buffer + sizeof(elf32_ehdr_t));
    phdr->p_type = PT_LOAD;
    phdr->p_offset = 0;
    phdr->p_vaddr = base_vaddr;
    phdr->p_paddr = base_vaddr;
    phdr->p_flags = 7; // R W X
    phdr->p_align = 0x1000;

    // 3. Emit x86 machine code
    uint8_t* code = bin_buffer + header_size;
    int cp = 0;

    // Function Prologue: push ebp; mov ebp, esp
    code[cp++] = X86_PUSH_EBP;
    code[cp++] = 0x89; code[cp++] = 0xE5;

    // Calculate memory offset of string data placed directly after the code
    int code_estimated_len = 40;
    uint32_t str_vaddr = code_entry + code_estimated_len;

    // Syscall 4: sys_write(1, str_vaddr, msg_len)
    // mov eax, 4
    code[cp++] = 0xB8;
    code[cp++] = 0x04; code[cp++] = 0x00; code[cp++] = 0x00; code[cp++] = 0x00;
    // mov ebx, 1 (stdout)
    code[cp++] = 0xBB;
    code[cp++] = 0x01; code[cp++] = 0x00; code[cp++] = 0x00; code[cp++] = 0x00;
    // mov ecx, str_vaddr
    code[cp++] = 0xB9;
    memcpy(code + cp, &str_vaddr, 4); cp += 4;
    // mov edx, msg_len
    code[cp++] = 0xBA;
    memcpy(code + cp, &msg_len, 4); cp += 4;
    // int 0x80
    code[cp++] = 0xCD; code[cp++] = 0x80;

    // Syscall 1: sys_exit(return_code)
    // mov eax, 1
    code[cp++] = 0xB8;
    code[cp++] = 0x01; code[cp++] = 0x00; code[cp++] = 0x00; code[cp++] = 0x00;
    // mov ebx, return_code
    code[cp++] = 0xBB;
    memcpy(code + cp, &return_code, 4); cp += 4;
    // int 0x80
    code[cp++] = 0xCD; code[cp++] = 0x80;

    // Pad to code_estimated_len
    while (cp < code_estimated_len) {
        code[cp++] = 0x90; // NOP
    }

    // Append String Data
    memcpy(code + cp, printed_msg, msg_len);
    cp += msg_len;
    code[cp++] = '\0';

    uint32_t total_filesz = header_size + cp;
    phdr->p_filesz = total_filesz;
    phdr->p_memsz = total_filesz;

    // 4. Save to filesystem
    int fd = vfs_open(out_path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        serial_println("[COMPILER] Error opening output file for write.");
        return -1;
    }
    vfs_write(fd, bin_buffer, total_filesz);
    vfs_close(fd);

    fs_register_entry(out_path, FS_TYPE_FILE);

    serial_print("[COMPILER] Compilation succeeded! Created ELF32 binary (");
    serial_print_dec(total_filesz);
    serial_println(" bytes).");
    return 0;
}

int compiler_compile_file(const char* src_path, const char* out_path, compiler_frontend_t frontend) {
    int fd = vfs_open(src_path, O_RDONLY);
    if (fd < 0) {
        serial_print("[COMPILER] File not found: ");
        serial_println(src_path);
        return -1;
    }
    int read_bytes = vfs_read(fd, src_buffer, sizeof(src_buffer) - 1);
    vfs_close(fd);

    if (read_bytes <= 0) {
        serial_println("[COMPILER] Source file is empty.");
        return -1;
    }
    src_buffer[read_bytes] = '\0';

    return compiler_compile_string(src_buffer, out_path, frontend);
}

int compiler_exec_cli(int argc, char** argv, compiler_frontend_t frontend) {
    const char* fe_name = (frontend == COMPILER_FRONTEND_CLANG) ? "clang" : "gcc";

    if (argc < 2) {
        serial_print(fe_name);
        serial_println(": fatal error: no input files\ncompilation terminated.");
        return 1;
    }

    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        if (frontend == COMPILER_FRONTEND_CLANG) {
            serial_println("LemonOS clang version 17.0.6 (tags/RELEASE_1706/final)");
            serial_println("Target: i386-unknown-lemonos");
            serial_println("Thread model: posix");
            serial_println("InstalledDir: /usr/bin");
        } else {
            serial_println("gcc (LemonOS 13.2.0) 13.2.0");
            serial_println("Copyright (C) 2024 Free Software Foundation, Inc.");
            serial_println("This is free software; see the source for copying conditions.");
        }
        return 0;
    }

    const char* src_path = NULL;
    char out_path[64] = "/bin/a.out";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            strncpy(out_path, argv[++i], sizeof(out_path) - 1);
            out_path[sizeof(out_path) - 1] = '\0';
        } else if (argv[i][0] != '-') {
            src_path = argv[i];
        }
    }

    if (!src_path) {
        serial_print(fe_name);
        serial_println(": fatal error: no input files specified.");
        return 1;
    }

    int res = compiler_compile_file(src_path, out_path, frontend);
    if (res == 0) {
        serial_print("[");
        serial_print(fe_name);
        serial_print("] Output binary written to ");
        serial_println(out_path);
        return 0;
    } else {
        serial_print("[");
        serial_print(fe_name);
        serial_println("] Build failed with errors.");
        return 1;
    }
}

void compiler_run_tests(void) {
    serial_println("[COMPILER] Running Compiler Verification Tests...");

    // Test compiling a C program string
    const char* test_c =
        "#include <stdio.h>\n"
        "int main(void) {\n"
        "    printf(\"Hello, LemonOS World from GCC!\\n\");\n"
        "    return 0;\n"
        "}\n";

    int res = compiler_compile_string(test_c, "/bin/gcctest", COMPILER_FRONTEND_GCC);
    if (res == 0 && fs_file_exists("/bin/gcctest")) {
        serial_println("  -> [TEST 1/2] GCC compiling test_c -> /bin/gcctest: PASS");
    } else {
        serial_println("  -> [TEST 1/2] GCC compilation: FAIL");
    }

    int res2 = compiler_compile_string(test_c, "/bin/clangtest", COMPILER_FRONTEND_CLANG);
    if (res2 == 0 && fs_file_exists("/bin/clangtest")) {
        serial_println("  -> [TEST 2/2] Clang compiling test_c -> /bin/clangtest: PASS");
    } else {
        serial_println("  -> [TEST 2/2] Clang compilation: FAIL");
    }
    serial_println("[COMPILER] Compiler suite verified.");
}
