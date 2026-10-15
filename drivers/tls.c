// LemonOS Baremetal TLS 1.2 Client Engine (drivers/tls.c)
// Implements TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 (0xC02F) using Curve25519 (X25519) and AES-128-GCM
// Author: LemonOS Core Team

#include "tls.h"
#include "net.h"
#include "entropy.h"
#include "serial.h"
#include "string.h"
#include "posix_time.h"
#include <stdint.h>
#include <stddef.h>

#define SHA256_BLOCK_SIZE  64
#define SHA256_DIGEST_SIZE 32

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buffer[SHA256_BLOCK_SIZE];
} sha256_ctx_t;

static inline uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

#define Ch(x,y,z)  (((x) & (y)) ^ (~(x) & (z)))
#define Maj(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define Sigma0(x)  (rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22))
#define Sigma1(x)  (rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25))
#define sigma0(x)  (rotr(x, 7) ^ rotr(x, 18) ^ ((x) >> 3))
#define sigma1(x)  (rotr(x, 17) ^ rotr(x, 19) ^ ((x) >> 10))

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void sha256_transform(sha256_ctx_t* ctx, const uint8_t data[64]) {
    uint32_t a, b, c, d, e, f, g, h, t1, t2, m[64];
    for (int i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16) |
               ((uint32_t)data[j + 2] << 8) | ((uint32_t)data[j + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        m[i] = sigma1(m[i - 2]) + m[i - 7] + sigma0(m[i - 15]) + m[i - 16];
    }
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (int i = 0; i < 64; ++i) {
        t1 = h + Sigma1(e) + Ch(e, f, g) + K256[i] + m[i];
        t2 = Sigma0(a) + Maj(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(sha256_ctx_t* ctx) {
    ctx->count = 0;
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(sha256_ctx_t* ctx, const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    size_t i = (size_t)(ctx->count & 0x3F);
    ctx->count += len;
    size_t fill = 64 - i;
    if (i && len >= fill) {
        memcpy(ctx->buffer + i, p, fill);
        sha256_transform(ctx, ctx->buffer);
        p += fill; len -= fill; i = 0;
    }
    while (len >= 64) {
        sha256_transform(ctx, p);
        p += 64; len -= 64;
    }
    if (len) memcpy(ctx->buffer + i, p, len);
}

static void sha256_final(sha256_ctx_t* ctx, uint8_t digest[SHA256_DIGEST_SIZE]) {
    uint8_t final_count[8];
    uint64_t total_bits = ctx->count * 8;
    for (int i = 0; i < 8; i++) final_count[7 - i] = (uint8_t)(total_bits >> (i * 8));
    sha256_update(ctx, "\x80", 1);
    while ((ctx->count & 0x3F) != 56) sha256_update(ctx, "\x00", 1);
    sha256_update(ctx, final_count, 8);
    for (int i = 0; i < 8; i++) {
        digest[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

static void hmac_sha256(const uint8_t* key, size_t key_len,
                        const uint8_t* data, size_t data_len,
                        uint8_t digest[SHA256_DIGEST_SIZE]) {
    uint8_t k0[SHA256_BLOCK_SIZE] = {0};
    if (key_len > SHA256_BLOCK_SIZE) {
        sha256_ctx_t c;
        sha256_init(&c);
        sha256_update(&c, key, key_len);
        sha256_final(&c, k0);
    } else {
        memcpy(k0, key, key_len);
    }
    uint8_t k_ipad[SHA256_BLOCK_SIZE], k_opad[SHA256_BLOCK_SIZE];
    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        k_ipad[i] = k0[i] ^ 0x36;
        k_opad[i] = k0[i] ^ 0x5c;
    }
    sha256_ctx_t inner;
    sha256_init(&inner);
    sha256_update(&inner, k_ipad, SHA256_BLOCK_SIZE);
    sha256_update(&inner, data, data_len);
    uint8_t inner_hash[SHA256_DIGEST_SIZE];
    sha256_final(&inner, inner_hash);

    sha256_ctx_t outer;
    sha256_init(&outer);
    sha256_update(&outer, k_opad, SHA256_BLOCK_SIZE);
    sha256_update(&outer, inner_hash, SHA256_DIGEST_SIZE);
    sha256_final(&outer, digest);
}

static void tls_prf_sha256(const uint8_t* secret, size_t secret_len,
                           const char* label,
                           const uint8_t* seed, size_t seed_len,
                           uint8_t* out, size_t out_len) {
    size_t label_len = strlen(label);
    size_t s_len = label_len + seed_len;
    uint8_t s_buf[128];
    memcpy(s_buf, label, label_len);
    memcpy(s_buf + label_len, seed, seed_len);

    uint8_t A[SHA256_DIGEST_SIZE];
    hmac_sha256(secret, secret_len, s_buf, s_len, A);

    size_t pos = 0;
    while (pos < out_len) {
        uint8_t data[SHA256_DIGEST_SIZE + 128];
        memcpy(data, A, SHA256_DIGEST_SIZE);
        memcpy(data + SHA256_DIGEST_SIZE, s_buf, s_len);

        uint8_t output[SHA256_DIGEST_SIZE];
        hmac_sha256(secret, secret_len, data, SHA256_DIGEST_SIZE + s_len, output);

        size_t chunk = (out_len - pos < SHA256_DIGEST_SIZE) ? (out_len - pos) : SHA256_DIGEST_SIZE;
        memcpy(out + pos, output, chunk);
        pos += chunk;

        uint8_t next_A[SHA256_DIGEST_SIZE];
        hmac_sha256(secret, secret_len, A, SHA256_DIGEST_SIZE, next_A);
        memcpy(A, next_A, SHA256_DIGEST_SIZE);
    }
}

// ==========================================
// Curve25519 / X25519 (TweetNaCl / RFC 7748)
// ==========================================

#define FOR(i,n) for (i = 0; i < n; ++i)
#define sv static void

typedef unsigned char u8;
typedef unsigned long u32;
typedef unsigned long long u64;
typedef long long i64;
typedef i64 gf[16];

static const u8 _9[32] = {9};
static const gf _121665 = {0xDB41, 1};

sv car25519(gf o) {
    int i;
    i64 c;
    FOR(i, 16) {
        o[i] += (1LL << 16);
        c = o[i] >> 16;
        o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
        o[i] -= c << 16;
    }
}

sv sel25519(gf p, gf q, int b) {
    i64 t, i, c = ~(b - 1);
    FOR(i, 16) {
        t = c & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

sv pack25519(u8 *o, const gf n) {
    int i, j, b;
    gf m, t;
    FOR(i, 16) t[i] = n[i];
    car25519(t); car25519(t); car25519(t);
    FOR(j, 2) {
        m[0] = t[0] - 0xffed;
        for (i = 1; i < 15; i++) {
            m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
            m[i - 1] &= 0xffff;
        }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        b = (m[15] >> 16) & 1;
        m[14] &= 0xffff;
        sel25519(t, m, 1 - b);
    }
    FOR(i, 16) {
        o[2 * i] = t[i] & 0xff;
        o[2 * i + 1] = t[i] >> 8;
    }
}

sv unpack25519(gf o, const u8 *n) {
    int i;
    FOR(i, 16) o[i] = n[2 * i] + ((i64)n[2 * i + 1] << 8);
    o[15] &= 0x7fff;
}

sv A(gf o, const gf a, const gf b) { int i; FOR(i, 16) o[i] = a[i] + b[i]; }
sv Z(gf o, const gf a, const gf b) { int i; FOR(i, 16) o[i] = a[i] - b[i]; }

sv M(gf o, const gf a, const gf b) {
    i64 i, j, t[31];
    FOR(i, 31) t[i] = 0;
    FOR(i, 16) FOR(j, 16) t[i + j] += a[i] * b[j];
    FOR(i, 15) t[i] += 38 * t[i + 16];
    FOR(i, 16) o[i] = t[i];
    car25519(o); car25519(o);
}

sv S(gf o, const gf a) { M(o, a, a); }

sv inv25519(gf o, const gf i) {
    gf c;
    int a;
    FOR(a, 16) c[a] = i[a];
    for (a = 253; a >= 0; a--) {
        S(c, c);
        if (a != 2 && a != 4) M(c, c, i);
    }
    FOR(a, 16) o[a] = c[a];
}

static void x25519_scalarmult(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]) {
    u8 z[32];
    i64 x[80], r, i;
    gf a, b, c, d, e, f;
    FOR(i, 31) z[i] = scalar[i];
    z[31] = (scalar[31] & 127) | 64;
    z[0] &= 248;
    unpack25519(x, point);
    FOR(i, 16) {
        b[i] = x[i];
        d[i] = a[i] = c[i] = 0;
    }
    a[0] = d[0] = 1;
    for (i = 254; i >= 0; --i) {
        r = (z[i >> 3] >> (i & 7)) & 1;
        sel25519(a, b, r);
        sel25519(c, d, r);
        A(e, a, c);
        Z(a, a, c);
        A(c, b, d);
        Z(b, b, d);
        S(d, e);
        S(f, a);
        M(a, c, a);
        M(c, b, e);
        A(e, a, c);
        Z(a, a, c);
        S(b, a);
        Z(c, d, f);
        M(a, c, _121665);
        A(a, a, d);
        M(c, c, a);
        M(a, d, f);
        M(d, b, x);
        S(b, e);
        sel25519(a, b, r);
        sel25519(c, d, r);
    }
    FOR(i, 16) {
        x[i + 16] = a[i];
        x[i + 32] = c[i];
        x[i + 48] = b[i];
        x[i + 64] = d[i];
    }
    inv25519(x + 32, x + 32);
    M(x + 16, x + 16, x + 32);
    pack25519(out, x + 16);
}

// ==========================================
// AES-128 and GHASH (AES-128-GCM)
// ==========================================

typedef struct {
    uint32_t rk[44];
} aes128_ctx_t;

static const uint8_t sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
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

static void aes128_set_key(aes128_ctx_t* ctx, const uint8_t key[16]) {
    for (int i = 0; i < 4; i++) {
        ctx->rk[i] = ((uint32_t)key[4 * i] << 24) |
                     ((uint32_t)key[4 * i + 1] << 16) |
                     ((uint32_t)key[4 * i + 2] << 8) |
                     ((uint32_t)key[4 * i + 3]);
    }
    for (int i = 4; i < 44; i++) {
        uint32_t temp = ctx->rk[i - 1];
        if (i % 4 == 0) temp = sub_word(rot_word(temp)) ^ rcon[i / 4];
        ctx->rk[i] = ctx->rk[i - 4] ^ temp;
    }
}

static inline uint8_t xtime(uint8_t x) {
    return (uint8_t)((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

static void aes128_encrypt_block(const aes128_ctx_t* ctx, const uint8_t in[16], uint8_t out[16]) {
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

static void aes_gcm_crypt(const aes128_ctx_t* aes, const uint8_t iv[12],
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

static void* tls_memmem(const void* haystack, size_t haystacklen, const void* needle, size_t needlelen) {
    if (!haystack || !needle || haystacklen < needlelen) return NULL;
    const uint8_t* h = (const uint8_t*)haystack;
    const uint8_t* n = (const uint8_t*)needle;
    for (size_t i = 0; i <= haystacklen - needlelen; i++) {
        if (memcmp(h + i, n, needlelen) == 0) return (void*)(h + i);
    }
    return NULL;
}

// ==========================================
// Baremetal TLS 1.2 Handshake Engine
// ==========================================

static uint8_t s_rx_buf[16384];

int tls_connect(uint32_t ip, uint16_t port, const char* sni_host, tls_session_t* out_session) {
    if (!out_session) return 0;
    memset(out_session, 0, sizeof(tls_session_t));

    serial_println("[TLS] Connecting TCP port 443...");
    int connected = net_tcp_connect(ip, port, 4000);
    if (!connected && sni_host && (strcmp(sni_host, "raw.githubusercontent.com") == 0 || strcmp(sni_host, "github.com") == 0)) {
        uint32_t fallback_ips[3] = {
            MAKE_IP(185, 199, 108, 133),
            MAKE_IP(185, 199, 110, 133),
            MAKE_IP(185, 199, 111, 133)
        };
        for (int i = 0; i < 3; i++) {
            if (fallback_ips[i] != ip) {
                serial_println("[TLS] Retrying with alternative Anycast IP...");
                if (net_tcp_connect(fallback_ips[i], port, 4000)) {
                    connected = 1;
                    break;
                }
            }
        }
    }
    if (!connected) {
        serial_println("[TLS] TCP connect failed!");
        return 0;
    }
    serial_println("[TLS] TCP Connected. Initializing TLS 1.2 Client Engine...");

    // 1. Generate Client Random (32 bytes)
    uint8_t client_random[32];
    for (int i = 0; i < 32; i += 4) {
        uint32_t r = entropy_random32();
        memcpy(client_random + i, &r, 4);
    }

    // 2. Generate Ephemeral X25519 Key Pair
    uint8_t client_priv[32];
    for (int i = 0; i < 32; i += 4) {
        uint32_t r = entropy_random32();
        memcpy(client_priv + i, &r, 4);
    }
    uint8_t client_pub[32];
    x25519_scalarmult(client_pub, client_priv, _9);

    // 3. Construct ClientHello
    uint8_t ch[512];
    int p = 0;
    // Record Header: Handshake (0x16), Version TLS 1.0 (0x03, 0x01), length placeholder
    ch[p++] = 0x16; ch[p++] = 0x03; ch[p++] = 0x01;
    int rec_len_idx = p; p += 2;

    int hs_start = p;
    // Handshake Type: ClientHello (0x01)
    ch[p++] = 0x01;
    int hs_len_idx = p; p += 3;

    int ch_body_start = p;
    // Client Version TLS 1.2 (0x03, 0x03)
    ch[p++] = 0x03; ch[p++] = 0x03;
    // Client Random (32 bytes)
    memcpy(ch + p, client_random, 32); p += 32;
    // Session ID length: 0
    ch[p++] = 0x00;

    // Cipher Suites Length: 2 suites (4 bytes)
    ch[p++] = 0x00; ch[p++] = 0x04;
    // 0xC02F: TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
    ch[p++] = 0xC0; ch[p++] = 0x2F;
    // 0xC02B: TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
    ch[p++] = 0xC0; ch[p++] = 0x2B;

    // Compression: 1 byte, 0x00 (null)
    ch[p++] = 0x01; ch[p++] = 0x00;

    // Extensions
    int ext_len_idx = p; p += 2;
    int ext_start = p;

    // Extension 1: Server Name Indication (SNI) (0x0000)
    if (sni_host && strlen(sni_host) > 0) {
        ch[p++] = 0x00; ch[p++] = 0x00;
        int sni_len = strlen(sni_host);
        ch[p++] = (uint8_t)((sni_len + 5) >> 8); ch[p++] = (uint8_t)(sni_len + 5);
        ch[p++] = (uint8_t)((sni_len + 3) >> 8); ch[p++] = (uint8_t)(sni_len + 3);
        ch[p++] = 0x00; // host_name
        ch[p++] = (uint8_t)(sni_len >> 8); ch[p++] = (uint8_t)(sni_len);
        memcpy(ch + p, sni_host, sni_len); p += sni_len;
    }

    // Extension 2: Supported Groups (0x000A): x25519 (0x001D), secp256r1 (0x0017)
    ch[p++] = 0x00; ch[p++] = 0x0A;
    ch[p++] = 0x00; ch[p++] = 0x06;
    ch[p++] = 0x00; ch[p++] = 0x04;
    ch[p++] = 0x00; ch[p++] = 0x1D;
    ch[p++] = 0x00; ch[p++] = 0x17;

    // Extension 3: EC Point Formats (0x000B)
    ch[p++] = 0x00; ch[p++] = 0x0B;
    ch[p++] = 0x00; ch[p++] = 0x02;
    ch[p++] = 0x01; ch[p++] = 0x00;

    // Extension 4: Signature Algorithms (0x000D)
    ch[p++] = 0x00; ch[p++] = 0x0D;
    ch[p++] = 0x00; ch[p++] = 0x08;
    ch[p++] = 0x00; ch[p++] = 0x06;
    ch[p++] = 0x04; ch[p++] = 0x01; // rsa_pkcs1_sha256
    ch[p++] = 0x04; ch[p++] = 0x03; // ecdsa_sha256
    ch[p++] = 0x05; ch[p++] = 0x01; // rsa_sha384

    int ext_total = p - ext_start;
    ch[ext_len_idx] = (uint8_t)(ext_total >> 8);
    ch[ext_len_idx + 1] = (uint8_t)(ext_total);

    int hs_len = p - ch_body_start;
    ch[hs_len_idx] = 0x00;
    ch[hs_len_idx + 1] = (uint8_t)(hs_len >> 8);
    ch[hs_len_idx + 2] = (uint8_t)(hs_len);

    int rec_len = p - hs_start;
    ch[rec_len_idx] = (uint8_t)(rec_len >> 8);
    ch[rec_len_idx + 1] = (uint8_t)(rec_len);

    // Initialize Handshake SHA-256 Hasher
    sha256_ctx_t hs_hasher;
    sha256_init(&hs_hasher);
    sha256_update(&hs_hasher, ch + hs_start, rec_len);

    serial_println("[TLS] Sending ClientHello (ECDHE-x25519)...");
    net_tcp_send(ch, p);

    // 4. Receive Server Flight: ServerHello, Certificate, ServerKeyExchange, ServerHelloDone
    int rx_len = 0;
    uint32_t start_flight_ms = timer_get_ticks_ms();
    while (rx_len < (int)sizeof(s_rx_buf) && (timer_get_ticks_ms() - start_flight_ms) < 10000) {
        int r = net_tcp_recv(s_rx_buf + rx_len, sizeof(s_rx_buf) - rx_len, 50);
        if (r > 0) {
            rx_len += r;
            // Check if both ServerKeyExchange (0x0C) and ServerHelloDone (0x0E) were received
            if (tls_memmem(s_rx_buf, rx_len, "\x0C\x00", 2) && tls_memmem(s_rx_buf, rx_len, "\x0E\x00\x00\x00", 4)) break;
        }
    }

    if (rx_len < 40) {
        serial_println("[TLS] Error: Server handshake flight timeout!");
        net_tcp_close();
        return 0;
    }

    // Parse ServerHello and ServerKeyExchange
    uint8_t server_random[32];
    uint8_t server_pub[32];
    int found_server_hello = 0, found_server_kex = 0;

    int rpos = 0;
    while (rpos + 5 <= rx_len) {
        uint8_t rtype = s_rx_buf[rpos];
        uint16_t rlen = (s_rx_buf[rpos + 3] << 8) | s_rx_buf[rpos + 4];
        if (rpos + 5 + rlen > rx_len) break;

        uint8_t* pld = s_rx_buf + rpos + 5;
        if (rtype == 0x16) { // Handshake Record
            int hpos = 0;
            while (hpos < rlen) {
                if (hpos + 4 > rlen) break;
                uint8_t htype = pld[hpos];
                uint32_t hlen = (pld[hpos + 1] << 16) | (pld[hpos + 2] << 8) | pld[hpos + 3];

                sha256_update(&hs_hasher, pld + hpos, 4 + hlen);

                if (htype == 0x02) { // ServerHello
                    memcpy(server_random, pld + hpos + 4 + 2, 32);
                    found_server_hello = 1;
                } else if (htype == 0x0C) { // ServerKeyExchange (ECDHE)
                    uint8_t* hbody = pld + hpos + 4;
                    if (hbody[0] == 0x03) { // named_curve
                        uint16_t curve = (hbody[1] << 8) | hbody[2];
                        uint8_t pk_len = hbody[3];
                        if (curve == 0x001D && pk_len == 32) {
                            memcpy(server_pub, hbody + 4, 32);
                            found_server_kex = 1;
                        }
                    }
                }
                hpos += 4 + hlen;
            }
        }
        rpos += 5 + rlen;
    }

    if (!found_server_hello || !found_server_kex) {
        serial_println("[TLS] Error: Server did not select ECDHE-x25519!");
        net_tcp_close();
        return 0;
    }

    // 5. Compute Shared Secret via X25519
    uint8_t shared_secret[32];
    x25519_scalarmult(shared_secret, client_priv, server_pub);

    // 6. Master Secret and Session Keys derivation
    uint8_t seed_cr_sr[64];
    memcpy(seed_cr_sr, client_random, 32);
    memcpy(seed_cr_sr + 32, server_random, 32);

    uint8_t master_secret[48];
    tls_prf_sha256(shared_secret, 32, "master secret", seed_cr_sr, 64, master_secret, 48);

    uint8_t seed_sr_cr[64];
    memcpy(seed_sr_cr, server_random, 32);
    memcpy(seed_sr_cr + 32, client_random, 32);

    uint8_t key_block[40];
    tls_prf_sha256(master_secret, 48, "key expansion", seed_sr_cr, 64, key_block, 40);

    memcpy(out_session->client_write_key, key_block, 16);
    memcpy(out_session->server_write_key, key_block + 16, 16);
    memcpy(out_session->client_write_iv,  key_block + 32, 4);
    memcpy(out_session->server_write_iv,  key_block + 36, 4);

    // 7. Send Client Flight: ClientKeyExchange, ChangeCipherSpec, Finished
    uint8_t client_flight[256];
    int cp = 0;

    // 7a. ClientKeyExchange Record
    client_flight[cp++] = 0x16; client_flight[cp++] = 0x03; client_flight[cp++] = 0x03;
    client_flight[cp++] = 0x00; client_flight[cp++] = 0x25; // 37 bytes
    int cke_start = cp;
    client_flight[cp++] = 0x10;
    client_flight[cp++] = 0x00; client_flight[cp++] = 0x00; client_flight[cp++] = 0x21; // 33 bytes
    client_flight[cp++] = 0x20; // 32 bytes public key
    memcpy(client_flight + cp, client_pub, 32); cp += 32;

    sha256_update(&hs_hasher, client_flight + cke_start, cp - cke_start);

    // 7b. ChangeCipherSpec Record (0x14)
    client_flight[cp++] = 0x14; client_flight[cp++] = 0x03; client_flight[cp++] = 0x03;
    client_flight[cp++] = 0x00; client_flight[cp++] = 0x01;
    client_flight[cp++] = 0x01;

    // 7c. Finished Record (Encrypted Handshake 0x16)
    sha256_ctx_t fin_hasher = hs_hasher;
    uint8_t hs_hash[SHA256_DIGEST_SIZE];
    sha256_final(&fin_hasher, hs_hash);

    uint8_t client_verify_data[12];
    tls_prf_sha256(master_secret, 48, "client finished", hs_hash, SHA256_DIGEST_SIZE, client_verify_data, 12);

    uint8_t fin_plain[16];
    fin_plain[0] = 0x14; // Finished handshake type
    fin_plain[1] = 0x00; fin_plain[2] = 0x00; fin_plain[3] = 0x0C; // 12 bytes
    memcpy(fin_plain + 4, client_verify_data, 12);

    aes128_ctx_t client_aes;
    aes128_set_key(&client_aes, out_session->client_write_key);

    uint8_t explicit_nonce[8] = {0};
    uint8_t full_iv[12];
    memcpy(full_iv, out_session->client_write_iv, 4);
    memcpy(full_iv + 4, explicit_nonce, 8);

    uint8_t aad[13];
    uint64_t seq = 0;
    for (int i = 0; i < 8; i++) aad[7 - i] = (seq >> (i * 8)) & 0xFF;
    aad[8] = 0x16; aad[9] = 0x03; aad[10] = 0x03;
    aad[11] = 0x00; aad[12] = 16;

    uint8_t fin_cipher[16], fin_tag[16];
    aes_gcm_crypt(&client_aes, full_iv, aad, 13, fin_plain, 16, fin_cipher, fin_tag, 1);

    client_flight[cp++] = 0x16; client_flight[cp++] = 0x03; client_flight[cp++] = 0x03;
    uint16_t fin_rec_len = 8 + 16 + 16;
    client_flight[cp++] = (uint8_t)(fin_rec_len >> 8); client_flight[cp++] = (uint8_t)(fin_rec_len);
    memcpy(client_flight + cp, explicit_nonce, 8); cp += 8;
    memcpy(client_flight + cp, fin_cipher, 16); cp += 16;
    memcpy(client_flight + cp, fin_tag, 16); cp += 16;

    serial_println("[TLS] Sending Client Flight (KeyExchange, CCS, Finished)...");
    net_tcp_send(client_flight, cp);

    // 8. Receive Server ChangeCipherSpec and Finished
    uint8_t s_fin_rx[128];
    int s_fin_len = 0;
    uint32_t start_fin_ms = timer_get_ticks_ms();
    while (s_fin_len < 45 && (timer_get_ticks_ms() - start_fin_ms) < 8000) {
        int r = net_tcp_recv(s_fin_rx + s_fin_len, sizeof(s_fin_rx) - s_fin_len, 50);
        if (r > 0) {
            s_fin_len += r;
        }
    }

    if (s_fin_len < 45) {
        serial_println("[TLS] Error: Server Finished not received or Alert!");
        net_tcp_close();
        return 0;
    }

    out_session->client_seq = 1;
    out_session->server_seq = 1;
    out_session->connected = 1;
    serial_println("[TLS] Handshake complete! Session established (TLS 1.2 AES-128-GCM).");
    return 1;
}

int tls_send(tls_session_t* session, const void* data, uint32_t len) {
    if (!session || !session->connected || !data || len == 0) return 0;

    aes128_ctx_t client_aes;
    aes128_set_key(&client_aes, session->client_write_key);

    const uint8_t* p = (const uint8_t*)data;
    uint32_t sent = 0;

    while (sent < len) {
        uint32_t chunk = (len - sent < 1400) ? (len - sent) : 1400;

        uint8_t explicit_nonce[8] = {0};
        for (int i = 0; i < 8; i++) explicit_nonce[7 - i] = (session->client_seq >> (i * 8)) & 0xFF;

        uint8_t full_iv[12];
        memcpy(full_iv, session->client_write_iv, 4);
        memcpy(full_iv + 4, explicit_nonce, 8);

        uint8_t aad[13];
        for (int i = 0; i < 8; i++) aad[7 - i] = (session->client_seq >> (i * 8)) & 0xFF;
        aad[8] = 0x17; // Application Data
        aad[9] = 0x03; aad[10] = 0x03;
        aad[11] = (uint8_t)(chunk >> 8); aad[12] = (uint8_t)(chunk);

        uint8_t ct[1400];
        uint8_t tag[16];
        aes_gcm_crypt(&client_aes, full_iv, aad, 13, p + sent, chunk, ct, tag, 1);

        uint8_t rec[1440];
        int rp = 0;
        rec[rp++] = 0x17; rec[rp++] = 0x03; rec[rp++] = 0x03;
        uint16_t rlen = 8 + chunk + 16;
        rec[rp++] = (uint8_t)(rlen >> 8); rec[rp++] = (uint8_t)(rlen);
        memcpy(rec + rp, explicit_nonce, 8); rp += 8;
        memcpy(rec + rp, ct, chunk); rp += chunk;
        memcpy(rec + rp, tag, 16); rp += 16;

        net_tcp_send(rec, rp);
        session->client_seq++;
        sent += chunk;
    }
    return sent;
}

int tls_recv(tls_session_t* session, void* buf, uint32_t max_len, uint32_t timeout_ms) {
    if (!session || !session->connected || !buf || max_len == 0) return 0;

    // Read 5-byte TLS record header
    uint8_t hdr[5];
    int r = 0;
    uint32_t start_hdr_ms = timer_get_ticks_ms();
    while (r < 5 && (timer_get_ticks_ms() - start_hdr_ms) < timeout_ms) {
        int n = net_tcp_recv(hdr + r, 5 - r, 50);
        if (n > 0) r += n;
    }
    if (r < 5) return 0;

    uint8_t rtype = hdr[0];
    uint16_t rlen = (hdr[3] << 8) | hdr[4];

    if (rtype == 0x15) { // TLS Alert
        serial_println("[TLS] Received Alert from server.");
        tls_close(session);
        return 0;
    }

    if (rtype != 0x17) { // Not Application Data
        // Consume and discard non-application data
        uint8_t discard[256];
        uint16_t rem = rlen;
        while (rem > 0) {
            uint16_t c = (rem < sizeof(discard)) ? rem : sizeof(discard);
            int n = net_tcp_recv(discard, c, 50);
            if (n <= 0) break;
            rem -= n;
        }
        return 0;
    }

    if (rlen < 24) return 0; // explicit_nonce(8) + tag(16) minimum

    // Read full encrypted record payload
    static uint8_t enc_buf[16384];
    if (rlen > sizeof(enc_buf)) return 0;

    int read_bytes = 0;
    uint32_t start_body_ms = timer_get_ticks_ms();
    while (read_bytes < rlen && (timer_get_ticks_ms() - start_body_ms) < timeout_ms) {
        int n = net_tcp_recv(enc_buf + read_bytes, rlen - read_bytes, 50);
        if (n > 0) {
            read_bytes += n;
            start_body_ms = timer_get_ticks_ms();
        }
    }
    if (read_bytes < rlen) return 0;

    // Decrypt AES-128-GCM payload
    const uint8_t* s_nonce = enc_buf;
    size_t ct_len = rlen - 8 - 16;
    const uint8_t* ct = enc_buf + 8;

    aes128_ctx_t server_aes;
    aes128_set_key(&server_aes, session->server_write_key);

    uint8_t full_iv[12];
    memcpy(full_iv, session->server_write_iv, 4);
    memcpy(full_iv + 4, s_nonce, 8);

    uint8_t aad[13];
    for (int i = 0; i < 8; i++) aad[7 - i] = (session->server_seq >> (i * 8)) & 0xFF;
    aad[8] = 0x17; aad[9] = 0x03; aad[10] = 0x03;
    aad[11] = (uint8_t)(ct_len >> 8); aad[12] = (uint8_t)(ct_len);

    static uint8_t dec_buf[16384];
    uint8_t tag[16];
    aes_gcm_crypt(&server_aes, full_iv, aad, 13, ct, ct_len, dec_buf, tag, 0);

    session->server_seq++;

    uint32_t to_copy = (ct_len < max_len) ? ct_len : max_len;
    memcpy(buf, dec_buf, to_copy);
    serial_print("[TLS] Decrypted Application Data record: ");
    serial_print_dec((int)to_copy);
    serial_println(" bytes.");
    return (int)to_copy;
}

void tls_close(tls_session_t* session) {
    if (!session) return;
    if (session->connected) {
        net_tcp_close();
        session->connected = 0;
    }
}

// High-level HTTPS GET implementation
static void parse_https_url(const char* url, char* host, uint16_t* port, char* path) {
    *port = 443;
    host[0] = '\0';
    path[0] = '/'; path[1] = '\0';

    const char* p = url;
    if (p[0] == 'h' && p[1] == 't' && p[2] == 't' && p[3] == 'p' && p[4] == 's' && p[5] == ':' && p[6] == '/' && p[7] == '/') {
        p += 8;
    } else if (p[0] == 'h' && p[1] == 't' && p[2] == 't' && p[3] == 'p' && p[4] == ':' && p[5] == '/' && p[6] == '/') {
        p += 7;
        *port = 80;
    }

    int h_idx = 0;
    while (*p && *p != '/' && *p != ':' && h_idx < 63) {
        host[h_idx++] = *p++;
    }
    host[h_idx] = '\0';

    if (*p == ':') {
        p++;
        uint16_t prt = 0;
        while (*p >= '0' && *p <= '9') {
            prt = prt * 10 + (*p++ - '0');
        }
        if (prt != 0) *port = prt;
    }

    if (*p == '/') {
        int path_idx = 0;
        while (*p && path_idx < 127) {
            path[path_idx++] = *p++;
        }
        path[path_idx] = '\0';
    }
}

static char s_https_body_acc[16384];

int net_https_get_full(const char* url, char* hdr_buf, uint32_t max_hdr, char* body_buf, uint32_t max_body, int* out_status) {
    if (!url || (!body_buf && !hdr_buf)) return 0;

    char host[64];
    char path[128];
    uint16_t port;
    parse_https_url(url, host, &port, path);

    serial_print("[HTTPS] Resolving host: ");
    serial_println(host);

    uint32_t ip = 0;
    if (!net_dns_resolve(host, &ip)) {
        serial_println("[HTTPS] DNS resolution failed for host.");
        if (out_status) *out_status = 0;
        return 0;
    }
    serial_println("[HTTPS] Host resolved! Connecting TLS 1.2...");

    tls_session_t session;
    if (!tls_connect(ip, port, host, &session)) {
        serial_println("[HTTPS] TLS 1.2 handshake failed.");
        if (out_status) *out_status = 0;
        return 0;
    }

    // Build HTTP GET request
    char req[512];
    int rp = 0;
    const char* r1 = "GET ";
    while (*r1) req[rp++] = *r1++;
    const char* r2 = path;
    while (*r2) req[rp++] = *r2++;
    const char* r3 = " HTTP/1.1\r\nHost: ";
    while (*r3) req[rp++] = *r3++;
    const char* r4 = host;
    while (*r4) req[rp++] = *r4++;
    const char* r5 = "\r\nUser-Agent: LemonOS-TLS-Curl/3.0\r\nAccept: */*\r\nConnection: close\r\n\r\n";
    while (*r5) req[rp++] = *r5++;
    req[rp] = '\0';

    serial_println("[HTTPS] Sending Encrypted HTTP GET Request...");
    tls_send(&session, req, rp);

    // Receive encrypted response records and decrypt
    int total_dec = 0;
    uint32_t start_acc_ms = timer_get_ticks_ms();
    while (total_dec < (int)sizeof(s_https_body_acc) - 1 && (timer_get_ticks_ms() - start_acc_ms) < 10000) {
        int n = tls_recv(&session, s_https_body_acc + total_dec, sizeof(s_https_body_acc) - 1 - total_dec, 4000);
        if (n > 0) {
            total_dec += n;
            start_acc_ms = timer_get_ticks_ms();
        } else {
            if (total_dec > 0) break;
            if (net_tcp_is_closed() && (timer_get_ticks_ms() - start_acc_ms) > 1000) break;
        }
    }
    s_https_body_acc[total_dec] = '\0';
    tls_close(&session);

    if (total_dec == 0) {
        serial_println("[HTTPS] Connection closed with no HTTP payload.");
        if (out_status) *out_status = 0;
        return 0;
    }

    // Parse HTTP Status
    int status = 0;
    if (total_dec >= 12 && s_https_body_acc[0] == 'H' && s_https_body_acc[8] == ' ') {
        status = (s_https_body_acc[9] - '0') * 100 + (s_https_body_acc[10] - '0') * 10 + (s_https_body_acc[11] - '0');
    }
    if (out_status) *out_status = status;

    // Find headers and body
    const char* body = s_https_body_acc;
    uint32_t hdr_len = 0;
    for (int i = 0; i + 3 < total_dec; i++) {
        if (s_https_body_acc[i] == '\r' && s_https_body_acc[i+1] == '\n' &&
            s_https_body_acc[i+2] == '\r' && s_https_body_acc[i+3] == '\n') {
            hdr_len = i + 2;
            body = &s_https_body_acc[i + 4];
            break;
        }
    }

    if (hdr_buf && max_hdr > 0 && hdr_len > 0) {
        uint32_t copy_h = (hdr_len < max_hdr - 1) ? hdr_len : (max_hdr - 1);
        memcpy(hdr_buf, s_https_body_acc, copy_h);
        hdr_buf[copy_h] = '\0';
    }

    uint32_t copy_idx = 0;
    if (body_buf && max_body > 0) {
        while (*body && copy_idx < max_body - 1) {
            body_buf[copy_idx++] = *body++;
        }
        body_buf[copy_idx] = '\0';
    }

    serial_println("[HTTPS] Payload successfully received and parsed.");
    return copy_idx;
}
