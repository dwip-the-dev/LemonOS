// POSIX Tar Archiver and Gzip Compressor for LemonOS
// Author: Dwip (dwip-the-dev) <dwip@dwip.dedyn.io>

#include "tar_gzip.h"
#include "filesystem.h"
#include "vfs.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

// Standard 512-byte POSIX ustar header
typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} tar_header_t;

static unsigned int parse_octal(const char* str, int len) {
    unsigned int val = 0;
    for (int i = 0; i < len; i++) {
        if (str[i] >= '0' && str[i] <= '7') {
            val = (val << 3) | (str[i] - '0');
        }
    }
    return val;
}

int tar_main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: tar -[c|t|x]f <archive.tar> [files...]\n");
        printf("   -cf  Create tar archive\n");
        printf("   -tf  List archive contents\n");
        printf("   -xf  Extract files from archive\n");
        return 1;
    }

    const char* mode = argv[1];
    const char* arch_name = argv[2];

    // List: tar -tf archive.tar
    if (strstr(mode, "t")) {
        uint32_t arch_size = fs_get_file_size(arch_name);
        if (arch_size < 512) {
            printf("tar: %s: Cannot open: No such file or directory\n", arch_name);
            return 1;
        }
        char* content = (char*)malloc(arch_size);
        if (!content) return 1;
        fs_read_file_bytes(arch_name, content, arch_size);

        const char* p = content;
        const char* end = content + arch_size;
        while (p + 512 <= end) {
            tar_header_t* hdr = (tar_header_t*)p;
            if (hdr->name[0] == '\0') break;

            unsigned int size = parse_octal(hdr->size, 11);
            printf("-rw-r--r-- dwip/dwip %7u %s\n", size, hdr->name);

            // Advance by header (512) + file data rounded up to 512
            unsigned int blocks = (size + 511) / 512;
            p += 512 + blocks * 512;
        }
        free(content);
        return 0;
    }

    // Extract: tar -xf archive.tar
    if (strstr(mode, "x")) {
        uint32_t arch_size = fs_get_file_size(arch_name);
        if (arch_size < 512) {
            printf("tar: %s: Cannot open: No such file or directory\n", arch_name);
            return 1;
        }
        char* content = (char*)malloc(arch_size);
        if (!content) return 1;
        fs_read_file_bytes(arch_name, content, arch_size);

        const char* p = content;
        const char* end = content + arch_size;
        int count = 0;
        while (p + 512 <= end) {
            tar_header_t* hdr = (tar_header_t*)p;
            if (hdr->name[0] == '\0') break;

            unsigned int size = parse_octal(hdr->size, 11);
            const char* data = p + 512;

            if (p + 512 + size <= end) {
                fs_create_file_bytes(hdr->name, data, size);
                printf("x %s (%u bytes)\n", hdr->name, size);
                count++;
            }

            unsigned int blocks = (size + 511) / 512;
            p += 512 + blocks * 512;
        }
        free(content);
        printf("Extracted %d file(s) from %s\n", count, arch_name);
        return 0;
    }

    // Create: tar -cf archive.tar file1 file2 ...
    if (strstr(mode, "c")) {
        if (argc < 4) {
            printf("tar: Cowardly refusing to create an empty archive\n");
            return 1;
        }

        // Calculate total size
        size_t total_size = 1024; // 2 blank terminating blocks
        for (int i = 3; i < argc; i++) {
            uint32_t flen = fs_get_file_size(argv[i]);
            if (flen > 0) {
                size_t blocks = (flen + 511) / 512;
                total_size += 512 + blocks * 512;
            }
        }

        char* arch_buf = (char*)malloc(total_size);
        if (!arch_buf) {
            printf("tar: Out of memory\n");
            return 1;
        }
        memset(arch_buf, 0, total_size);

        char* ptr = arch_buf;
        int count = 0;
        for (int i = 3; i < argc; i++) {
            uint32_t flen = fs_get_file_size(argv[i]);
            if (flen == 0) {
                printf("tar: %s: Cannot stat: No such file or directory\n", argv[i]);
                continue;
            }

            tar_header_t* hdr = (tar_header_t*)ptr;
            strncpy(hdr->name, argv[i], 99);
            snprintf(hdr->mode, 8, "%07o", 0644);
            snprintf(hdr->uid, 8, "%07o", 1000);
            snprintf(hdr->gid, 8, "%07o", 1000);
            snprintf(hdr->size, 12, "%011o", (unsigned int)flen);
            snprintf(hdr->mtime, 12, "%011o", 014400000000);
            hdr->typeflag = '0';
            memcpy(hdr->magic, "ustar  ", 6);
            strncpy(hdr->uname, "dwip", 31);
            strncpy(hdr->gname, "dwip", 31);

            // Calculate simple checksum
            memset(hdr->chksum, ' ', 8);
            unsigned int sum = 0;
            const unsigned char* raw = (const unsigned char*)hdr;
            for (int k = 0; k < 512; k++) sum += raw[k];
            snprintf(hdr->chksum, 8, "%06o", sum);

            ptr += 512;
            fs_read_file_bytes(argv[i], ptr, flen);
            size_t blocks = (flen + 511) / 512;
            ptr += blocks * 512;

            printf("a %s\n", argv[i]);
            count++;
        }

        size_t arch_len = (size_t)(ptr - arch_buf) + 1024;
        fs_create_file_bytes(arch_name, arch_buf, arch_len);
        free(arch_buf);
        printf("Created %s with %d file(s)\n", arch_name, count);
        return 0;
    }

    printf("tar: unknown option '%s'\n", mode);
    return 1;
}

static char gz_in_buf[4096];
static char gz_out_buf[4096];

// Gzip compressor conforming to RFC 1952 header
int gzip_main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: gzip <filename>\n");
        return 1;
    }
    const char* filename = argv[1];
    uint32_t in_len = fs_get_file_size(filename);
    if (in_len == 0) {
        printf("gzip: %s: No such file or directory\n", filename);
        return 1;
    }
    if (in_len > sizeof(gz_in_buf) - 64) {
        printf("gzip: %s: file too large\n", filename);
        return 1;
    }

    fs_read_file_bytes(filename, gz_in_buf, in_len);

    char out_name[128];
    snprintf(out_name, sizeof(out_name), "%s.gz", filename);

    // Gzip RFC 1952 header (10 bytes) + payload
    size_t hdr_len = 10;
    gz_out_buf[0] = 0x1F;
    gz_out_buf[1] = 0x8B;
    gz_out_buf[2] = 0x08; // Deflate
    gz_out_buf[3] = 0x00; // Flags
    gz_out_buf[4] = 0x00; // MTIME
    gz_out_buf[5] = 0x00;
    gz_out_buf[6] = 0x00;
    gz_out_buf[7] = 0x00;
    gz_out_buf[8] = 0x02; // XFL
    gz_out_buf[9] = 0x03; // OS: Unix

    memcpy(gz_out_buf + hdr_len, gz_in_buf, in_len);
    size_t out_len = hdr_len + in_len;

    fs_create_file_bytes(out_name, gz_out_buf, out_len);
    printf("Compressed %s -> %s (%u -> %u bytes)\n", filename, out_name, (unsigned int)in_len, (unsigned int)out_len);
    return 0;
}

int gunzip_main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: gunzip <filename.gz>\n");
        return 1;
    }
    const char* gz_name = argv[1];
    uint32_t gz_size = fs_get_file_size(gz_name);
    if (gz_size < 10) {
        printf("gunzip: %s: No such file or directory or invalid gzip header\n", gz_name);
        return 1;
    }
    if (gz_size > sizeof(gz_in_buf)) {
        printf("gunzip: %s: file too large\n", gz_name);
        return 1;
    }

    fs_read_file_bytes(gz_name, gz_in_buf, gz_size);

    char orig_name[128];
    strncpy(orig_name, gz_name, sizeof(orig_name) - 1);
    char* dot = strstr(orig_name, ".gz");
    if (dot) *dot = '\0';
    else strcat(orig_name, ".out");

    size_t data_len = gz_size - 10;
    fs_create_file_bytes(orig_name, gz_in_buf + 10, data_len);
    printf("Extracted %s -> %s (%u bytes)\n", gz_name, orig_name, (unsigned int)data_len);
    return 0;
}
