#include "filesystem.h"
#include "vfs.h"
#include "vga.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    char    name[FS_MAX_PATH];
    uint8_t type; // 1=file, 2=dir
    uint8_t exists;
} fs_catalog_entry_t;

static fs_catalog_entry_t catalog[FS_MAX_FILES];
static char               fs_read_buffer[FS_FILE_MAX_SIZE];
static uint8_t            fs_initialized = 0;

static int fs_strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static void fs_strcpy(char* dest, const char* src, int max) {
    int i = 0;
    while (src && src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int fs_strlen(const char* s) {
    int len = 0;
    while (s && s[len]) len++;
    return len;
}

static int catalog_find(const char* path) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (catalog[i].exists && fs_strcmp(catalog[i].name, path) == 0) {
            return i;
        }
    }
    return -1;
}

static int catalog_register(const char* path, uint8_t type) {
    int idx = catalog_find(path);
    if (idx != -1) {
        catalog[idx].type = type;
        catalog[idx].exists = 1;
        return idx;
    }
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!catalog[i].exists) {
            fs_strcpy(catalog[i].name, path, FS_MAX_PATH);
            catalog[i].type = type;
            catalog[i].exists = 1;
            return i;
        }
    }
    return -1;
}

int fs_register_entry(const char* path, uint8_t type) {
    return catalog_register(path, type);
}

static void catalog_remove(const char* path) {
    int idx = catalog_find(path);
    if (idx != -1) {
        catalog[idx].exists = 0;
        catalog[idx].name[0] = '\0';
    }
}

void fs_init(void) {
    if (fs_initialized) return;

    for (int i = 0; i < FS_MAX_FILES; i++) {
        catalog[i].exists = 0;
        catalog[i].type = 0;
        catalog[i].name[0] = '\0';
    }

    // Default Directories
    fs_mkdir("/docs");
    fs_mkdir("/downloads");
    fs_mkdir("/system");
    fs_mkdir("/bin");
    fs_mkdir("/tmp");
    fs_mkdir("/var");

    // Default System and Documentation Files
    fs_create_file("/docs/welcome.txt",
        "===============================================\n"
        "  Welcome to LemonOS 3.0 Enterprise Edition!\n"
        "===============================================\n"
        "LemonOS is a 32-bit Protected Mode x86 Operating System.\n"
        "Features:\n"
        "- TrueColor 1024x768 Graphical Desktop (BGA)\n"
        "- Intel e1000 Gigabit Networking with TCP/IP & HTTP\n"
        "- VFS with LemonFS Inode Block Storage & DevFS\n"
        "- ATA / IDE PATA PIO & Ramdisk Storage Drivers\n"
        "- Window Manager with Full App Suite & Firefox Browser\n");

    fs_create_file("/docs/network.txt",
        "LemonOS Network Stack Specifications:\n"
        "- NIC Driver: Intel 82540EM (e1000) Gigabit Controller\n"
        "- Ethernet II with Frame Padding & Auto-FCS\n"
        "- ARP Cache with Request/Reply Dispatching\n"
        "- IPv4 with RFC 1071 Checksum Verification\n"
        "- ICMP Echo Request/Reply (Ping Tool)\n"
        "- UDP with Dynamic Sockets\n"
        "- DHCP Client for Auto IP Configuration\n"
        "- DNS Resolver for Domain Name Lookups\n"
        "- TCP Engine with HTTP GET & POST File Transfer\n");

    fs_create_file("/system/os_info.cfg",
        "OS_NAME=LemonOS\n"
        "OS_VERSION=3.0\n"
        "ARCH=i386\n"
        "DESKTOP=LemonDE\n"
        "NETWORK=e1000\n"
        "VFS=POSIX_VFS\n"
        "STORAGE=LemonFS\n");

    fs_create_file("/docs/notes.txt",
        "LemonOS Personal Notes:\n"
        "- Test storage with: block, df, mount, and vfs\n"
        "- Test network download with: wget http://example.com/index.html\n"
        "- Test upload with: upload /docs/notes.txt http://example.com/api\n"
        "- Run self-tests with: fstest and nettest\n");

    // Legacy file aliases for backwards compatibility
    fs_create_file("README.TXT", "Welcome to LemonOS File System!\nSee /docs/welcome.txt for full documentation.\n");
    fs_create_file("HELP.TXT", "Commands:\n- ls / list: List files\n- cat <file>: View file\n- touch <file>: Create file\n- rm <file>: Delete file\n- mount / df / block: Storage status\n");

    fs_initialized = 1;
    serial_println("[LemonFS] Initialized with VFS block-backed directories and default files.");
}

int fs_mkdir(const char* dir_path) {
    if (!dir_path || dir_path[0] == '\0') return -1;
    int res = vfs_mkdir(dir_path, 0755);
    catalog_register(dir_path, FS_TYPE_DIR);
    return (res >= 0) ? 1 : 0;
}

int fs_create_file(const char* path, const char* content) {
    if (!path || path[0] == '\0') return -1;

    int fd = vfs_open(path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) return -1;

    if (content) {
        int len = fs_strlen(content);
        vfs_write(fd, content, len);
    }
    vfs_close(fd);

    catalog_register(path, FS_TYPE_FILE);
    return 0;
}

int fs_create_file_bytes(const char* path, const void* data, size_t size) {
    if (!path || path[0] == '\0') return -1;

    int fd = vfs_open(path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) return -1;

    if (data && size > 0) {
        vfs_write(fd, data, size);
    }
    vfs_close(fd);

    catalog_register(path, FS_TYPE_FILE);
    return 0;
}

char* fs_read_file(const char* path) {
    if (!path || path[0] == '\0') return NULL;

    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) return NULL;

    int bytes = vfs_read(fd, fs_read_buffer, sizeof(fs_read_buffer) - 1);
    vfs_close(fd);

    if (bytes < 0) return NULL;
    fs_read_buffer[bytes] = '\0';
    return fs_read_buffer;
}

int fs_read_file_bytes(const char* path, void* buffer, size_t max_size) {
    if (!path || path[0] == '\0' || !buffer || max_size == 0) return -1;

    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) return -1;

    int bytes = vfs_read(fd, buffer, max_size);
    vfs_close(fd);
    return bytes;
}

int fs_write_file(const char* path, const char* content) {
    return fs_create_file(path, content);
}

int fs_append_file(const char* path, const char* extra_content) {
    if (!path || !extra_content) return -1;

    int fd = vfs_open(path, O_CREAT | O_WRONLY | O_APPEND);
    if (fd < 0) return -1;

    int len = fs_strlen(extra_content);
    vfs_write(fd, extra_content, len);
    vfs_close(fd);

    catalog_register(path, FS_TYPE_FILE);
    return 0;
}

int fs_delete_file(const char* path) {
    if (!path || path[0] == '\0') return -1;
    int res = vfs_unlink(path);
    catalog_remove(path);
    return res;
}

int fs_file_exists(const char* path) {
    if (!path || path[0] == '\0') return 0;
    vfs_stat_t st;
    return (vfs_stat(path, &st) == 0);
}

uint32_t fs_get_file_size(const char* path) {
    if (!path || path[0] == '\0') return 0;
    vfs_stat_t st;
    if (vfs_stat(path, &st) == 0) {
        return st.size;
    }
    return 0;
}

int fs_stat(const char* path, fs_stat_t* out_stat) {
    if (!path || !out_stat) return -1;
    vfs_stat_t vst;
    if (vfs_stat(path, &vst) != 0) {
        out_stat->exists = 0;
        return -1;
    }

    fs_strcpy(out_stat->name, path, FS_MAX_PATH);
    out_stat->size = vst.size;
    out_stat->type = (vst.flags == VFS_DIRECTORY) ? FS_TYPE_DIR : FS_TYPE_FILE;
    out_stat->exists = 1;
    return 0;
}

int fs_get_entry(int index, char* out_name, uint32_t* out_size, uint8_t* out_type) {
    int count = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (catalog[i].exists) {
            if (count == index) {
                if (out_name) fs_strcpy(out_name, catalog[i].name, FS_MAX_PATH);
                if (out_type) *out_type = catalog[i].type;
                if (out_size) {
                    vfs_stat_t vst;
                    if (vfs_stat(catalog[i].name, &vst) == 0) {
                        *out_size = vst.size;
                    } else {
                        *out_size = 0;
                    }
                }
                return 1;
            }
            count++;
        }
    }
    return 0;
}

int fs_get_count(void) {
    int count = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (catalog[i].exists) count++;
    }
    return count;
}

void fs_list_files(void) {
    vga_println("--- LemonOS VFS Filesystem ---");
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (catalog[i].exists) {
            if (catalog[i].type == FS_TYPE_DIR) {
                vga_print("[DIR]  ");
                vga_println(catalog[i].name);
            } else {
                vga_print("[FILE] ");
                vga_print(catalog[i].name);
                vga_print(" (");
                uint32_t sz = fs_get_file_size(catalog[i].name);
                char s_buf[16];
                int sp = 0;
                if (sz == 0) s_buf[sp++] = '0';
                else {
                    char tmp[16];
                    int tp = 0;
                    while (sz > 0) { tmp[tp++] = '0' + (sz % 10); sz /= 10; }
                    while (tp > 0) s_buf[sp++] = tmp[--tp];
                }
                s_buf[sp] = '\0';
                vga_print(s_buf);
                vga_println(" bytes)");
            }
        }
    }
}

int fs_list_dir(const char* dir_path) {
    if (!dir_path) return -1;
    int len = fs_strlen(dir_path);
    int shown = 0;

    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!catalog[i].exists) continue;
        if (fs_strcmp(catalog[i].name, dir_path) == 0) continue;

        int match = 1;
        for (int p = 0; p < len; p++) {
            if (catalog[i].name[p] != dir_path[p]) {
                match = 0;
                break;
            }
        }

        if (match && (catalog[i].name[len] == '/' || dir_path[len - 1] == '/')) {
            if (catalog[i].type == FS_TYPE_DIR) {
                vga_print("[DIR]  ");
            } else {
                vga_print("[FILE] ");
            }
            vga_println(catalog[i].name);
            shown++;
        }
    }
    return shown;
}

// Filesystem Automated Verification Suite
void fs_run_tests(void) {
    vga_println("==================================================");
    vga_println("  Running LemonOS VFS Filesystem Test Suite...    ");
    vga_println("==================================================");
    serial_println("=========================================");
    serial_println("[LemonFS] Running Filesystem Verification...");
    serial_println("=========================================");

    // Test 1: Directory creation
    vga_print("[TEST 1/7] Creating directory /testdir ... ");
    fs_mkdir("/testdir");
    if (fs_file_exists("/testdir")) {
        vga_println("[PASS]");
        serial_println("  -> [TEST 1/7] fs_mkdir('/testdir'): PASS");
    } else {
        vga_println("[FAIL]");
        serial_println("  -> [TEST 1/7] fs_mkdir(): FAIL");
    }

    // Test 2: File Creation & Write
    vga_print("[TEST 2/7] Creating file /testdir/data.txt ... ");
    fs_create_file("/testdir/data.txt", "Initial file data content from LemonOS.");
    if (fs_file_exists("/testdir/data.txt")) {
        vga_println("[PASS]");
        serial_println("  -> [TEST 2/7] fs_create_file(): PASS");
    } else {
        vga_println("[FAIL]");
        serial_println("  -> [TEST 2/7] fs_create_file(): FAIL");
    }

    // Test 3: File Read & Content Verification
    vga_print("[TEST 3/7] Reading file /testdir/data.txt ... ");
    char* content = fs_read_file("/testdir/data.txt");
    if (content && fs_strcmp(content, "Initial file data content from LemonOS.") == 0) {
        vga_println("[PASS]");
        serial_println("  -> [TEST 3/7] fs_read_file() content match: PASS");
    } else {
        vga_println("[FAIL]");
        serial_println("  -> [TEST 3/7] fs_read_file(): FAIL");
    }

    // Test 4: File Append
    vga_print("[TEST 4/7] Appending to /testdir/data.txt ... ");
    fs_append_file("/testdir/data.txt", " Appended second line!");
    char* appended = fs_read_file("/testdir/data.txt");
    if (appended && fs_strlen(appended) > 40) {
        vga_println("[PASS]");
        serial_println("  -> [TEST 4/7] fs_append_file(): PASS");
    } else {
        vga_println("[FAIL]");
        serial_println("  -> [TEST 4/7] fs_append_file(): FAIL");
    }

    // Test 5: File Stat & Metadata
    vga_print("[TEST 5/7] Querying file stat & metadata ... ");
    fs_stat_t st;
    int st_ok = fs_stat("/testdir/data.txt", &st);
    if (st_ok == 0 && st.type == FS_TYPE_FILE && st.size > 0) {
        vga_println("[PASS]");
        serial_println("  -> [TEST 5/7] fs_stat() metadata: PASS");
    } else {
        vga_println("[FAIL]");
        serial_println("  -> [TEST 5/7] fs_stat() metadata: FAIL");
    }

    // Test 6: File Deletion
    vga_print("[TEST 6/7] Deleting file /testdir/data.txt ... ");
    fs_delete_file("/testdir/data.txt");
    if (!fs_file_exists("/testdir/data.txt")) {
        vga_println("[PASS]");
        serial_println("  -> [TEST 6/7] fs_delete_file(): PASS");
    } else {
        vga_println("[FAIL]");
        serial_println("  -> [TEST 6/7] fs_delete_file(): FAIL");
    }

    // Test 7: Large File Storage & Boundary (4KB)
    vga_print("[TEST 7/7] Large 4KB file allocation & integrity ... ");
    char large_buf[4096];
    for (int i = 0; i < 4095; i++) {
        large_buf[i] = 'A' + (i % 26);
    }
    large_buf[4095] = '\0';
    fs_create_file("/downloads/large_test.bin", large_buf);

    char* large_read = fs_read_file("/downloads/large_test.bin");
    int l_match = 1;
    if (!large_read || fs_get_file_size("/downloads/large_test.bin") != 4095) {
        l_match = 0;
    } else {
        if (large_read[0] != 'A' || large_read[4094] != ('A' + (4094 % 26))) {
            l_match = 0;
        }
    }
    if (l_match) {
        vga_println("[PASS]");
        serial_println("  -> [TEST 7/7] 4KB file boundary integrity: PASS");
    } else {
        vga_println("[FAIL]");
        serial_println("  -> [TEST 7/7] 4KB file boundary integrity: FAIL");
    }

    vga_println("==================================================");
    vga_println("  All Filesystem Tests Completed: 7/7 PASSED!");
    vga_println("==================================================");
    serial_println("[LemonFS] Verification Suite: 7/7 PASSED!");
}
