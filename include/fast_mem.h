#ifndef FAST_MEM_H
#define FAST_MEM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Fast assembly memory operations utilizing x86 string instructions and SIMD
void*    fast_memcpy(void* dest, const void* src, size_t count);
void     fast_memset32(void* dest, uint32_t val, size_t count);

// Fast assembly graphics blitting and alpha blending for 60 FPS compositor
void     fast_alpha_blend_row(uint32_t* dst, uint32_t fg_color, uint8_t alpha, size_t count);
void     fast_blit_rect(uint32_t* dst, int dst_stride, const uint32_t* src, int src_stride, int w, int h);

// Hardware Time Stamp Counter (RDTSC) for cycle-accurate CPU profiling
uint64_t cpu_rdtsc(void);

// Query CPU hardware vendor string via CPUID (12 characters + null)
void     cpu_id_vendor(char* out_vendor);

// Run memory bandwidth benchmark (returns measured throughput in MB/s)
uint32_t cpu_perf_benchmark(void);

#ifdef __cplusplus
}
#endif

#endif // FAST_MEM_H
