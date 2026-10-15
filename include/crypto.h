#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>

// SHA-1 (RFC 3174) - 160-bit digest (used by Git)
#define SHA1_BLOCK_SIZE  64
#define SHA1_DIGEST_SIZE 20

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[SHA1_BLOCK_SIZE];
} sha1_ctx_t;

void sha1_init(sha1_ctx_t* ctx);
void sha1_update(sha1_ctx_t* ctx, const uint8_t* data, size_t len);
void sha1_final(sha1_ctx_t* ctx, uint8_t digest[SHA1_DIGEST_SIZE]);
void sha1_hash(const uint8_t* data, size_t len, uint8_t digest[SHA1_DIGEST_SIZE]);
void sha1_hash_str(const char* str, char out_hex[41]);

// SHA-256 (FIPS 180-2) - 256-bit digest
#define SHA256_BLOCK_SIZE  64
#define SHA256_DIGEST_SIZE 32

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  data[SHA256_BLOCK_SIZE];
    uint32_t datalen;
} sha256_ctx_t;

void sha256_init(sha256_ctx_t* ctx);
void sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len);
void sha256_final(sha256_ctx_t* ctx, uint8_t digest[SHA256_DIGEST_SIZE]);
void sha256_hash(const uint8_t* data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]);
void sha256_hash_str(const char* str, char out_hex[65]);

// MD5 (RFC 1321) - 128-bit digest (used by APT package verification)
#define MD5_BLOCK_SIZE  64
#define MD5_DIGEST_SIZE 16

typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t  buffer[MD5_BLOCK_SIZE];
} md5_ctx_t;

void md5_init(md5_ctx_t* ctx);
void md5_update(md5_ctx_t* ctx, const uint8_t* data, size_t len);
void md5_final(md5_ctx_t* ctx, uint8_t digest[MD5_DIGEST_SIZE]);
void md5_hash(const uint8_t* data, size_t len, uint8_t digest[MD5_DIGEST_SIZE]);
void md5_hash_str(const char* str, char out_hex[33]);

// HMAC-SHA256
void hmac_sha256(const uint8_t* key, size_t key_len,
                 const uint8_t* data, size_t data_len,
                 uint8_t digest[SHA256_DIGEST_SIZE]);

// Verification self-test suite
void crypto_run_tests(void);

#endif
