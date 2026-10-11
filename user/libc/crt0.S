# Process entry. The kernel leaves argc at (%rsp), argv[] after it, then NULL and an empty envp.
    .globl _start
    .text
_start:
    xor %ebp, %ebp
    mov (%rsp), %rdi          # argc
    lea 8(%rsp), %rsi         # argv
    and $-16, %rsp
    call main
    mov %eax, %edi
    call exit
1:  jmp 1b
    .section .note.GNU-stack,"",@progbits
