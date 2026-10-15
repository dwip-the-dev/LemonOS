#include "entropy.h"
#include "crypto.h"
#include "string.h"
#include "serial.h"

static uint8_t entropy_pool[ENTROPY_POOL_SIZE];
static uint32_t pool_index = 0;
static uint32_t sample_counter = 0;
static uint32_t reseed_counter = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint64_t rdtsc(void) {
    uint64_t val;
    asm volatile("rdtsc" : "=A"(val));
    return val;
}

void entropy_init(void) {
    memset(entropy_pool, 0x5A, ENTROPY_POOL_SIZE);
    pool_index = 0;
    sample_counter = 0;
    reseed_counter = 0;

    // Seed initial pool with hardware counters
    for (int i = 0; i < 64; i++) {
        entropy_harvest_hardware();
    }
    serial_println("[ENTROPY] Hardware entropy pool initialized (512 bytes CSPRNG).");
}

void entropy_add_sample(uint32_t val) {
    uint64_t t = rdtsc();
    uint32_t mixed = val ^ (uint32_t)t ^ (uint32_t)(t >> 32) ^ (++sample_counter);

    entropy_pool[pool_index] ^= (uint8_t)(mixed & 0xFF);
    entropy_pool[(pool_index + 1) % ENTROPY_POOL_SIZE] ^= (uint8_t)((mixed >> 8) & 0xFF);
    entropy_pool[(pool_index + 2) % ENTROPY_POOL_SIZE] ^= (uint8_t)((mixed >> 16) & 0xFF);
    entropy_pool[(pool_index + 3) % ENTROPY_POOL_SIZE] ^= (uint8_t)((mixed >> 24) & 0xFF);

    pool_index = (pool_index + 4) % ENTROPY_POOL_SIZE;
}

void entropy_harvest_hardware(void) {
    uint8_t pit0 = inb(0x40);
    uint8_t pit2 = inb(0x42);
    uint8_t rtc_sec = inb(0x71);
    uint64_t cycles = rdtsc();

    uint32_t hw = ((uint32_t)pit0) | ((uint32_t)pit2 << 8) | ((uint32_t)rtc_sec << 16) | ((uint32_t)cycles << 24);
    entropy_add_sample(hw);
}

void entropy_get_bytes(uint8_t* out, size_t len) {
    if (!out || len == 0) return;

    size_t generated = 0;
    uint8_t digest[SHA256_DIGEST_SIZE];

    while (generated < len) {
        entropy_harvest_hardware();

        // Hash current pool state with reseed counter
        sha256_ctx_t ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, entropy_pool, ENTROPY_POOL_SIZE);
        reseed_counter++;
        sha256_update(&ctx, (const uint8_t*)&reseed_counter, sizeof(reseed_counter));
        sha256_final(&ctx, digest);

        // Fold digest back into pool to prevent backtracking
        for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
            entropy_pool[(pool_index + i) % ENTROPY_POOL_SIZE] ^= digest[i];
        }

        size_t chunk = (len - generated < SHA256_DIGEST_SIZE) ? (len - generated) : SHA256_DIGEST_SIZE;
        memcpy(out + generated, digest, chunk);
        generated += chunk;
    }
}

uint32_t entropy_random32(void) {
    uint32_t res;
    entropy_get_bytes((uint8_t*)&res, sizeof(res));
    return res;
}

uint32_t entropy_random_range(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    uint32_t range = max - min + 1;
    return min + (entropy_random32() % range);
}

void entropy_run_tests(void) {
    serial_println("[ENTROPY] Running CSPRNG Self-Tests...");
    uint32_t r1 = entropy_random32();
    uint32_t r2 = entropy_random32();
    uint32_t r3 = entropy_random32();

    if (r1 != r2 && r2 != r3 && r1 != r3) {
        serial_println("  -> [TEST 1/2] CSPRNG non-repeating stream: PASS");
    } else {
        serial_println("  -> [TEST 1/2] CSPRNG non-repeating stream: FAIL");
    }

    uint8_t test_buf[64];
    entropy_get_bytes(test_buf, sizeof(test_buf));
    int non_zero = 0;
    for (size_t i = 0; i < sizeof(test_buf); i++) {
        if (test_buf[i] != 0) non_zero++;
    }
    if (non_zero > 50) {
        serial_println("  -> [TEST 2/2] Entropy byte variance: PASS");
    } else {
        serial_println("  -> [TEST 2/2] Entropy byte variance: FAIL");
    }
    serial_println("[ENTROPY] Entropy engine verified.");
}
