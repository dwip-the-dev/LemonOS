.section .text
.align 4

# -------------------------------------------------------------
# GDT & TSS Flush
# -------------------------------------------------------------
.global gdt_flush_asm
gdt_flush_asm:
    mov 4(%esp), %eax
    lgdt (%eax)

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    ljmp $0x08, $.flush_cs

.flush_cs:
    ret

.global tss_flush_asm
tss_flush_asm:
    mov $0x28, %ax
    ltr %ax
    ret

# -------------------------------------------------------------
# IDT Load
# -------------------------------------------------------------
.global idt_load_asm
idt_load_asm:
    mov 4(%esp), %eax
    lidt (%eax)
    ret

# -------------------------------------------------------------
# Syscall Vector 0x80 Handler
# -------------------------------------------------------------
.global syscall_entry_asm
.extern syscall_dispatcher
syscall_entry_asm:
    # Push dummy error code and int number so stack matches registers_t exactly
    push $0                 # err_code
    push $0x80              # int_no

    # Save user registers
    pusha
    push %ds
    push %es
    push %fs
    push %gs

    # Load kernel data segment
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    # Call C dispatcher with pointer to stack frame
    push %esp
    call syscall_dispatcher
    add $4, %esp

    # Overwrite saved EAX with return value
    # Stack layout: gs(0), fs(4), es(8), ds(12),
    #               edi(16), esi(20), ebp(24), esp(28), ebx(32), edx(36), ecx(40), eax(44),
    #               int_no(48), err_code(52), eip(56), cs(60), eflags(64), useresp(68), ss(72)
    mov %eax, 44(%esp)

    # Restore registers and return to caller
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    add $8, %esp            # Pop int_no and err_code
    iret

# -------------------------------------------------------------
# Jump to Ring 3 (User Mode)
# -------------------------------------------------------------
.global jump_to_ring3
jump_to_ring3:
    # Arguments: 4(%esp) = entry_point, 8(%esp) = user_stack
    mov 4(%esp), %ecx       # EIP
    mov 8(%esp), %edx       # ESP

    # Set user data segments (0x20 | 3 = 0x23)
    mov $0x23, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    # Build iret stack frame
    push $0x23              # SS (User Data)
    push %edx               # ESP (User Stack)
    pushf                   # EFLAGS
    pop %eax
    and $~0x200, %eax       # Ensure IF=0 (no unhandled hardware IRQ in user mode)
    or $0x3002, %eax        # IOPL=3 (allow I/O and int 0x80)
    push %eax
    push $0x1B              # CS (User Code: 0x18 | 3)
    push %ecx               # EIP (Entry Point)

    # Context switch to Ring 3
    iret

# -------------------------------------------------------------
# Setjmp / Longjmp for Userspace Process Return
# -------------------------------------------------------------
.global user_setjmp
user_setjmp:
    mov 4(%esp), %edx       # pointer to jmp_buf_t
    mov %ebx, 0(%edx)
    mov %esi, 4(%edx)
    mov %edi, 8(%edx)
    mov %ebp, 12(%edx)
    lea 4(%esp), %eax       # caller ESP before call
    mov %eax, 16(%edx)
    mov (%esp), %eax        # return EIP
    mov %eax, 20(%edx)
    xor %eax, %eax          # return 0
    ret

.global user_longjmp
user_longjmp:
    mov 4(%esp), %edx       # pointer to jmp_buf_t
    mov 8(%esp), %eax       # return code
    test %eax, %eax
    jnz .longjmp_non_zero
    mov $1, %eax
.longjmp_non_zero:
    # Restore kernel segments
    mov $0x10, %cx
    mov %cx, %ds
    mov %cx, %es
    mov %cx, %fs
    mov %cx, %gs
    mov %cx, %ss

    mov 0(%edx), %ebx
    mov 4(%edx), %esi
    mov 8(%edx), %edi
    mov 12(%edx), %ebp
    mov 16(%edx), %esp       # restore stack pointer
    jmp *20(%edx)           # jump to saved return EIP

# -------------------------------------------------------------
# Exception ISR Stubs
# -------------------------------------------------------------
.extern exception_handler

.macro ISR_NOERR num
.global isr_stub_\num
isr_stub_\num:
    push $0                 # Dummy error code
    push $\num              # Interrupt number
    jmp isr_common
.endm

.macro ISR_ERR num
.global isr_stub_\num
isr_stub_\num:
    push $\num              # Error code already on stack, push int number
    jmp isr_common
.endm

isr_common:
    pusha
    push %ds
    push %es
    push %fs
    push %gs

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    push %esp
    call exception_handler
    add $4, %esp

    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    add $8, %esp            # Pop error code and int number
    iret

# Exception Definitions
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

# -------------------------------------------------------------
# Dynamic Linker Lazy Runtime PLT Resolver Trampoline
# Standard System V i386 ABI specification:
# Called when a function is called via its PLT slot for lazy binding.
#
# Upon entry via PLT[0]:
#   0(%esp): mod_ptr       (pushed by PLT0 from GOT[1])
#   4(%esp): reloc_offset  (pushed by func@plt)
#   8(%esp): return address to original caller
#   12(%esp)...: arguments to the function
# -------------------------------------------------------------
.global _dl_runtime_resolve
.extern dynlink_runtime_resolve
_dl_runtime_resolve:
    push %eax
    push %ecx
    push %edx

    # Arguments to dynlink_runtime_resolve(mod_ptr, reloc_offset)
    # Stack layout now:
    #   0(%esp) = edx
    #   4(%esp) = ecx
    #   8(%esp) = eax
    #   12(%esp) = mod_ptr
    #   16(%esp) = reloc_offset
    #   20(%esp) = return address
    push 16(%esp)       # Push reloc_offset
    push 16(%esp)       # Push mod_ptr
    call dynlink_runtime_resolve
    add $8, %esp        # Pop arguments

    # %eax now holds the resolved target function address
    # Overwrite the reloc_offset slot on stack with the resolved function address
    mov %eax, 16(%esp)

    # Restore registers
    pop %edx
    pop %ecx
    pop %eax

    # Stack layout now:
    #   0(%esp) = mod_ptr
    #   4(%esp) = resolved function pointer
    #   8(%esp) = return address to caller
    add $4, %esp        # Pop mod_ptr
    ret                 # Jump to resolved function via RET (pops target into EIP)!
