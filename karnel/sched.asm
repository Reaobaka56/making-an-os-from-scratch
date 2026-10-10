bits 64
extern thread_exit_c

section .text

; void switch_context(uint64_t* old_rsp, uint64_t new_rsp)
global switch_context
switch_context:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov [rdi], rsp
    mov rsp, rsi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; First code run by a new thread: r12 = function, r13 = argument.
global thread_trampoline
thread_trampoline:
    sti
    mov rdi, r13
    call r12
    xor edi, edi
    call thread_exit_c
.hang:
    hlt
    jmp .hang

; void enter_usermode(uint64_t rip, uint64_t rsp)
global enter_usermode
enter_usermode:
    cli
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    push qword 0x1B        ; ss
    push rsi               ; rsp
    push qword 0x202       ; rflags (IF set)
    push qword 0x23        ; cs
    push rdi               ; rip
    iretq

section .note.GNU-stack noalloc noexec nowrite progbits
