.section .text
.global setjmp
.global longjmp

setjmp:
    mov 4(%esp), %edx       # jmp_buf pointer
    mov %ebx, 0(%edx)
    mov %esi, 4(%edx)
    mov %edi, 8(%edx)
    mov %ebp, 12(%edx)
    lea 4(%esp), %eax       # ESP before call
    mov %eax, 16(%edx)
    mov (%esp), %eax        # Return EIP
    mov %eax, 20(%edx)
    xor %eax, %eax          # return 0
    ret

longjmp:
    mov 4(%esp), %edx       # jmp_buf pointer
    mov 8(%esp), %eax       # return value
    test %eax, %eax
    jnz .non_zero
    mov $1, %eax            # Cannot return 0
.non_zero:
    mov 0(%edx), %ebx
    mov 4(%edx), %esi
    mov 8(%edx), %edi
    mov 12(%edx), %ebp
    mov 16(%edx), %esp
    mov 20(%edx), %ecx      # Return EIP
    jmp *%ecx
