#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// AES-128 and GCM from test_tls.c
typedef struct {
    uint32_t rk[44];
} aes128_ctx_t;

static const uint8_t sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static const uint32_t rcon[11] = {
    0x00000000, 0x01000000, 0x02000000, 0x04000000, 0x08000000,
    0x10000000, 0x20000000, 0x40000000, 0x80000000, 0x1B000000, 0x36000000
};

static uint32_t sub_word(uint32_t w) {
    return ((uint32_t)sbox[(w >> 24) & 0xFF] << 24) |
           ((uint32_t)sbox[(w >> 16) & 0xFF] << 16) |
           ((uint32_t)sbox[(w >>  8) & 0xFF] <<  8) |
           ((uint32_t)sbox[ w        & 0xFF]);
}

static uint32_t rot_word(uint32_t w) {
    return (w << 8) | (w >> 24);
}

void aes128_set_key(aes128_ctx_t* ctx, const uint8_t key[16]) {
    for (int i = 0; i < 4; i++) {
        ctx->rk[i] = ((uint32_t)key[4 * i] << 24) |
                     ((uint32_t)key[4 * i + 1] << 16) |
                     ((uint32_t)key[4 * i + 2] << 8) |
                     ((uint32_t)key[4 * i + 3]);
    }
    for (int i = 4; i < 44; i++) {
        uint32_t temp = ctx->rk[i - 1];
        if (i % 4 == 0) {
            temp = sub_word(rot_word(temp)) ^ rcon[i / 4];
        }
        ctx->rk[i] = ctx->rk[i - 4] ^ temp;
    }
}

static uint8_t xtime(uint8_t x) {
    return (uint8_t)((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

void aes128_encrypt_block(const aes128_ctx_t* ctx, const uint8_t in[16], uint8_t out[16]) {
    uint8_t state[4][4];
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            state[r][c] = in[r + 4 * c] ^ (uint8_t)(ctx->rk[c] >> (24 - 8 * r));

    for (int round = 1; round <= 10; round++) {
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                state[r][c] = sbox[state[r][c]];

        uint8_t t;
        t = state[1][0]; state[1][0] = state[1][1]; state[1][1] = state[1][2]; state[1][2] = state[1][3]; state[1][3] = t;
        t = state[2][0]; state[2][0] = state[2][2]; state[2][2] = t;
        t = state[2][1]; state[2][1] = state[2][3]; state[2][3] = t;
        t = state[3][3]; state[3][3] = state[3][2]; state[3][2] = state[3][1]; state[3][1] = state[3][0]; state[3][0] = t;

        if (round < 10) {
            for (int c = 0; c < 4; c++) {
                uint8_t a[4];
                for (int r = 0; r < 4; r++) a[r] = state[r][c];
                state[0][c] = xtime(a[0]) ^ xtime(a[1]) ^ a[1] ^ a[2] ^ a[3];
                state[1][c] = a[0] ^ xtime(a[1]) ^ xtime(a[2]) ^ a[2] ^ a[3];
                state[2][c] = a[0] ^ a[1] ^ xtime(a[2]) ^ xtime(a[3]) ^ a[3];
                state[3][c] = xtime(a[0]) ^ a[0] ^ a[1] ^ a[2] ^ xtime(a[3]);
            }
        }

        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                state[r][c] ^= (uint8_t)(ctx->rk[round * 4 + c] >> (24 - 8 * r));
    }

    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            out[r + 4 * c] = state[r][c];
}

static void ghash_mult(uint8_t x[16], const uint8_t y[16]) {
    uint8_t v[16], z[16] = {0};
    memcpy(v, y, 16);
    for (int i = 0; i < 128; i++) {
        if ((x[i / 8] >> (7 - (i % 8))) & 1) {
            for (int j = 0; j < 16; j++) z[j] ^= v[j];
        }
        int lsb = v[15] & 1;
        for (int j = 15; j > 0; j--) v[j] = (v[j] >> 1) | (v[j - 1] << 7);
        v[0] >>= 1;
        if (lsb) v[0] ^= 0xe1;
    }
    memcpy(x, z, 16);
}

void aes_gcm_crypt(const aes128_ctx_t* aes, const uint8_t iv[12],
                   const uint8_t* aad, size_t aad_len,
                   const uint8_t* in, size_t in_len,
                   uint8_t* out, uint8_t tag[16], int encrypt) {
    uint8_t H[16] = {0};
    aes128_encrypt_block(aes, H, H);

    uint8_t J0[16];
    memcpy(J0, iv, 12);
    J0[12] = 0; J0[13] = 0; J0[14] = 0; J0[15] = 1;

    uint8_t CB[16];
    memcpy(CB, J0, 16);

    size_t pos = 0;
    while (pos < in_len) {
        for (int i = 15; i >= 12; i--) {
            if (++CB[i] != 0) break;
        }
        uint8_t mask[16];
        aes128_encrypt_block(aes, CB, mask);
        size_t block_len = (in_len - pos < 16) ? (in_len - pos) : 16;
        for (size_t b = 0; b < block_len; b++) {
            out[pos + b] = in[pos + b] ^ mask[b];
        }
        pos += block_len;
    }

    const uint8_t* ciphertext = encrypt ? out : in;
    uint8_t S[16] = {0};

    for (size_t i = 0; i < aad_len; i += 16) {
        uint8_t block[16] = {0};
        size_t chunk = (aad_len - i < 16) ? (aad_len - i) : 16;
        memcpy(block, aad + i, chunk);
        for (int j = 0; j < 16; j++) S[j] ^= block[j];
        ghash_mult(S, H);
    }

    for (size_t i = 0; i < in_len; i += 16) {
        uint8_t block[16] = {0};
        size_t chunk = (in_len - i < 16) ? (in_len - i) : 16;
        memcpy(block, ciphertext + i, chunk);
        for (int j = 0; j < 16; j++) S[j] ^= block[j];
        ghash_mult(S, H);
    }

    uint8_t len_block[16];
    uint64_t aad_bits = (uint64_t)aad_len * 8;
    uint64_t ct_bits = (uint64_t)in_len * 8;
    for (int i = 0; i < 8; i++) {
        len_block[7 - i] = (aad_bits >> (i * 8)) & 0xFF;
        len_block[15 - i] = (ct_bits >> (i * 8)) & 0xFF;
    }
    for (int j = 0; j < 16; j++) S[j] ^= len_block[j];
    ghash_mult(S, H);

    uint8_t tag_mask[16];
    aes128_encrypt_block(aes, J0, tag_mask);
    for (int j = 0; j < 16; j++) tag[j] = S[j] ^ tag_mask[j];
}

void print_hex(const char* label, const uint8_t* d, size_t n) {
    printf("%s: ", label);
    for (size_t i = 0; i < n; i++) printf("%02x", d[i]);
    printf("\n");
}

int main() {
    // NIST SP 800-38D Test Case 1:
    // Key: all 0, IV: all 0, PT: empty, AAD: empty
    // Tag: 58e2fccefa7e3061367f1d57a4e7455a
    uint8_t key[16] = {0};
    uint8_t iv[12] = {0};
    uint8_t tag[16];
    aes128_ctx_t ctx;
    aes128_set_key(&ctx, key);
    aes_gcm_crypt(&ctx, iv, NULL, 0, NULL, 0, NULL, tag, 1);
    print_hex("Test Case 1 Tag", tag, 16);
    printf("Expected Tag   : 58e2fccefa7e3061367f1d57a4e7455a\n\n");

    // Test Case 2:
    // PT: 16 bytes 0
    // CT:  0388dace60b6a392f328c2b971b2fe78
    // Tag: ab6e47d42cec13bdf53a67b21257bddf
    uint8_t pt2[16] = {0};
    uint8_t ct2[16];
    aes_gcm_crypt(&ctx, iv, NULL, 0, pt2, 16, ct2, tag, 1);
    print_hex("Test Case 2 CT ", ct2, 16);
    printf("Expected CT    : 0388dace60b6a392f328c2b971b2fe78\n");
    print_hex("Test Case 2 Tag", tag, 16);
    printf("Expected Tag   : ab6e47d42cec13bdf53a67b21257bddf\n");

    test_aad(); return 0;
}

// Test Case 4 with AAD:
// Key: feffe9928665731c6d6a8f9467308308
// IV:  cafebabefacedbaddecaf888
// PT:  (empty)
// AAD: feedfacedeadbeeffeedfacedeadbeefabaddad2
// Tag: 2519498e80f1478f37492d00bf0ab944
static uint8_t hex2byte(char a, char b) {
    char s[3] = {a, b, 0};
    return (uint8_t)strtoul(s, NULL, 16);
}
static void hex2buf(const char* hex, uint8_t* out, size_t len) {
    for (size_t i = 0; i < len; i++) out[i] = hex2byte(hex[2*i], hex[2*i+1]);
}
void test_aad() {
    uint8_t k[16], iv[12], aad[20], exp_tag[16];
    hex2buf("feffe9928665731c6d6a8f9467308308", k, 16);
    hex2buf("cafebabefacedbaddecaf888", iv, 12);
    hex2buf("feedfacedeadbeeffeedfacedeadbeefabaddad2", aad, 20);
    hex2buf("2519498e80f1478f37492d00bf0ab944", exp_tag, 16);
    aes128_ctx_t ctx;
    aes128_set_key(&ctx, k);
    uint8_t tag[16];
    aes_gcm_crypt(&ctx, iv, aad, 20, NULL, 0, NULL, tag, 1);
    print_hex("Test Case AAD Tag", tag, 16);
    print_hex("Expected Tag     ", exp_tag, 16);
}
