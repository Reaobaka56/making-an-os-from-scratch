
; Kernel entry. Limine hands over control in 64-bit long mode, paging on.
bits 64

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kmain

_start:
    cli
    lea rsp, [rel stack_top]
    xor rbp, rbp
    call kmain
.hang:
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
