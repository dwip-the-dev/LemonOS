.section .text
.global _start
.extern main
.extern exit
.extern environ
.extern __libc_init_stdio

_start:
    # Clear frame pointer for stack traces
    xor %ebp, %ebp

    # Pop argc from stack
    pop %esi            # esi = argc (callee-saved)

    # ESP now points to argv[0]
    mov %esp, %edi      # edi = argv (callee-saved)

    # Calculate envp: argv + (argc + 1) * 4
    lea 4(%esp, %esi, 4), %ebx   # ebx = envp (callee-saved)

    # Save environ pointer
    mov %ebx, environ

    # Initialize standard I/O
    call __libc_init_stdio

    # Align stack to 16 bytes (System V ABI)
    and $~15, %esp
    sub $16, %esp
    mov %esi, 0(%esp)   # arg 1: argc
    mov %edi, 4(%esp)   # arg 2: argv
    mov %ebx, 8(%esp)   # arg 3: envp

    # Invoke main(argc, argv, envp)
    call main

    # Exit with return value from main
    mov %eax, 0(%esp)
    call exit

    # Failsafe if exit ever returns
    mov $1, %eax
    xor %ebx, %ebx
    int $0x80

.hang:
    hlt
    jmp .hang
