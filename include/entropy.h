#ifndef ENTROPY_H
#define ENTROPY_H

#include <stdint.h>
#include <stddef.h>

#define ENTROPY_POOL_SIZE 512

void     entropy_init(void);
void     entropy_add_sample(uint32_t val);
void     entropy_harvest_hardware(void);
void     entropy_get_bytes(uint8_t* out, size_t len);
uint32_t entropy_random32(void);
uint32_t entropy_random_range(uint32_t min, uint32_t max);

// Self-test suite
void     entropy_run_tests(void);

#endif
