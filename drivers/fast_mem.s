# =======================================================================
# LemonOS Core Assembly Optimization Subsystem
# High-throughput memory copy, memory set, CPUID probe & RDTSC profiling
# Author: Dwip Biswas (dwip-the-dev) <dwip@dwip.dedyn.io>
# =======================================================================

.section .data
bench_src:
    .fill 16384, 1, 0xAA
bench_dst:
    .fill 16384, 1, 0x00

.section .text
.global fast_memcpy
.global fast_memset32
.global fast_alpha_blend_row
.global fast_blit_rect
.global cpu_rdtsc
.global cpu_id_vendor
.global cpu_perf_benchmark

# -----------------------------------------------------------------------
# void fast_alpha_blend_row(uint32_t* dst, uint32_t fg_color, uint8_t alpha, size_t count)
# -----------------------------------------------------------------------
fast_alpha_blend_row:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    movl 8(%ebp), %edi       # dst
    movl 12(%ebp), %edx      # fg_color (0x00RRGGBB)
    movzbl 16(%ebp), %eax    # alpha (0..255)
    movl 20(%ebp), %ecx      # count

    testl %ecx, %ecx
    jle .Lblend_done

    testl %eax, %eax
    jz .Lblend_done          # alpha == 0: no-op

    cmpl $255, %eax
    jne .Lblend_calc

    # alpha == 255: pure overwrite
    pushl %ecx
    pushl %edx
    pushl %edi
    call fast_memset32
    addl $12, %esp
    jmp .Lblend_done

.Lblend_calc:
    # eax = alpha
    # ebx = inv_alpha (255 - alpha)
    movl $255, %ebx
    subl %eax, %ebx

    # Extract fg components
    # fg_r: (edx >> 16) & 0xFF
    # fg_g: (edx >> 8) & 0xFF
    # fg_b: edx & 0xFF
    movl %edx, %esi
    shrl $16, %esi
    andl $0xFF, %esi         # esi = fg_r
    imull %eax, %esi         # esi = fg_r * alpha

    # fg_g_alpha in stack frame temporary: push fg_g*alpha and fg_b*alpha
    movl %edx, %ecx
    shrl $8, %ecx
    andl $0xFF, %ecx
    imull %eax, %ecx         # ecx = fg_g * alpha
    pushl %ecx               # -16(%ebp): fg_g * alpha

    movl %edx, %ecx
    andl $0xFF, %ecx
    imull %eax, %ecx         # ecx = fg_b * alpha
    pushl %ecx               # -20(%ebp): fg_b * alpha

    pushl %esi               # -24(%ebp): fg_r * alpha
    pushl %ebx               # -28(%ebp): inv_alpha

    movl 20(%ebp), %ecx      # restore count

.Lblend_loop:
    movl (%edi), %edx        # edx = current bg pixel (0xXXRRGGBB)

    # 1. Red channel: ((bg_r * inv_alpha) + fg_r_alpha) / 255
    movl %edx, %eax
    shrl $16, %eax
    andl $0xFF, %eax
    imull -28(%ebp), %eax    # bg_r * inv_alpha
    addl -24(%ebp), %eax     # + fg_r_alpha
    # Fast divide by 255: (x + 1 + (x >> 8)) >> 8
    movl %eax, %esi
    shrl $8, %esi
    leal 1(%eax, %esi), %esi
    shrl $8, %esi
    andl $0xFF, %esi
    shll $16, %esi           # esi = out_r << 16

    # 2. Green channel: ((bg_g * inv_alpha) + fg_g_alpha) / 255
    movl %edx, %eax
    shrl $8, %eax
    andl $0xFF, %eax
    imull -28(%ebp), %eax    # bg_g * inv_alpha
    addl -16(%ebp), %eax     # + fg_g_alpha
    movl %eax, %ebx
    shrl $8, %ebx
    leal 1(%eax, %ebx), %ebx
    shrl $8, %ebx
    andl $0xFF, %ebx
    shll $8, %ebx
    orl %ebx, %esi           # esi = (out_r << 16) | (out_g << 8)

    # 3. Blue channel: ((bg_b * inv_alpha) + fg_b_alpha) / 255
    movl %edx, %eax
    andl $0xFF, %eax
    imull -28(%ebp), %eax    # bg_b * inv_alpha
    addl -20(%ebp), %eax     # + fg_b_alpha
    movl %eax, %ebx
    shrl $8, %ebx
    leal 1(%eax, %ebx), %ebx
    shrl $8, %ebx
    andl $0xFF, %ebx
    orl %ebx, %esi

    # Opaque alpha channel (0xFF000000)
    orl $0xFF000000, %esi
    movl %esi, (%edi)

    addl $4, %edi
    decl %ecx
    jnz .Lblend_loop

    addl $16, %esp           # clear stack temporary variables

.Lblend_done:
    popl %edi
    popl %esi
    popl %ebx
    movl %ebp, %esp
    popl %ebp
    ret

# -----------------------------------------------------------------------
# void fast_blit_rect(uint32_t* dst, int dst_stride, const uint32_t* src, int src_stride, int w, int h)
# -----------------------------------------------------------------------
fast_blit_rect:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    movl 8(%ebp), %edi       # dst
    movl 12(%ebp), %eax      # dst_stride (pixels)
    movl 16(%ebp), %esi      # src
    movl 20(%ebp), %ebx      # src_stride (pixels)
    movl 24(%ebp), %edx      # width (pixels)
    movl 28(%ebp), %ecx      # height (rows)

    testl %edx, %edx
    jle .Lblit_done
    testl %ecx, %ecx
    jle .Lblit_done

    # Calculate row step deltas in bytes: (stride - width) * 4
    subl %edx, %eax
    shll $2, %eax            # dst delta
    subl %edx, %ebx
    shll $2, %ebx            # src delta

.Lblit_row_loop:
    pushl %ecx               # preserve height counter
    movl %edx, %ecx          # ecx = width dwords
    rep movsl
    addl %eax, %edi          # dst += dst delta
    addl %ebx, %esi          # src += src delta
    popl %ecx                # restore height counter
    decl %ecx
    jnz .Lblit_row_loop

.Lblit_done:
    popl %edi
    popl %esi
    popl %ebx
    movl %ebp, %esp
    popl %ebp
    ret

# -----------------------------------------------------------------------
# void* fast_memcpy(void* dest, const void* src, size_t count)
# -----------------------------------------------------------------------
fast_memcpy:
    pushl %esi
    pushl %edi

    movl 12(%esp), %edi      # dest
    movl 16(%esp), %esi      # src
    movl 20(%esp), %ecx      # count (in bytes)

    # Check for zero or negative count
    testl %ecx, %ecx
    jle .Lmemcpy_done

    # Save original dest for return value
    movl %edi, %eax

    # Fast 32-bit dword transfer
    shrl $2, %ecx            # ecx = count / 4
    rep movsl

    # Handle remaining 0-3 tail bytes
    movl 20(%esp), %ecx
    andl $3, %ecx
    rep movsb

.Lmemcpy_done:
    popl %edi
    popl %esi
    ret

# -----------------------------------------------------------------------
# void fast_memset32(void* dest, uint32_t val, size_t count)
# -----------------------------------------------------------------------
fast_memset32:
    pushl %edi

    movl 8(%esp), %edi       # dest
    movl 12(%esp), %eax      # val (32-bit pixel or pattern)
    movl 16(%esp), %ecx      # count (number of 32-bit words)

    testl %ecx, %ecx
    jle .Lmemset_done

    rep stosl

.Lmemset_done:
    popl %edi
    ret

# -----------------------------------------------------------------------
# uint64_t cpu_rdtsc(void)
# Returns 64-bit hardware Time Stamp Counter in edx:eax
# -----------------------------------------------------------------------
cpu_rdtsc:
    rdtsc
    ret

# -----------------------------------------------------------------------
# void cpu_id_vendor(char* out_vendor)
# Queries CPUID(0) to get 12-char vendor string (e.g. GenuineIntel)
# -----------------------------------------------------------------------
cpu_id_vendor:
    pushl %ebx
    pushl %edi

    movl 12(%esp), %edi      # out_vendor buffer (at least 13 bytes)
    testl %edi, %edi
    jz .Lcpuid_done

    xorl %eax, %eax
    cpuid

    # CPUID(0) returns vendor in EBX, EDX, ECX
    movl %ebx, 0(%edi)
    movl %edx, 4(%edi)
    movl %ecx, 8(%edi)
    movb $0, 12(%edi)

.Lcpuid_done:
    popl %edi
    popl %ebx
    ret

# -----------------------------------------------------------------------
# uint32_t cpu_perf_benchmark(void)
# Measures memory copy speed over multiple iterations using RDTSC
# -----------------------------------------------------------------------
cpu_perf_benchmark:
    pushl %esi
    pushl %edi
    pushl %ebx

    # Warm up cache
    movl $bench_dst, %edi
    movl $bench_src, %esi
    movl $4096, %ecx
    rep movsl

    # Read start TSC
    rdtsc
    movl %eax, %ebx

    # Run 64 iterations of 16KB copy = 1MB total transfer
    movl $64, %edx
.Lbench_loop:
    movl $bench_dst, %edi
    movl $bench_src, %esi
    movl $4096, %ecx
    rep movsl
    decl %edx
    jnz .Lbench_loop

    # Read end TSC
    rdtsc
    subl %ebx, %eax          # eax = elapsed cycles for 1MB

    # Estimate throughput in MB/s (approx 2.4 GHz clock reference)
    # throughput = (2400000000 / cycles)
    testl %eax, %eax
    jz .Lbench_zero
    movl %eax, %ecx
    movl $2400000, %eax
    xorl %edx, %edx
    divl %ecx
    jmp .Lbench_exit

.Lbench_zero:
    movl $2400, %eax

.Lbench_exit:
    popl %ebx
    popl %edi
    popl %esi
    ret
