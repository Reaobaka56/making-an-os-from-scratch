bits 64
extern syscall_dispatch

section .data
global g_syscall_kstack
g_syscall_kstack: dq 0
g_saved_user_rsp: dq 0

section .text
; SYSCALL entry. rax = number, rdi/rsi/rdx = args. rcx = user rip, r11 = user rflags.
; Interrupts are masked on entry (SFMASK), so the shared scratch slot is safe.
global syscall_entry
syscall_entry:
    mov [rel g_saved_user_rsp], rsp
    mov rsp, [rel g_syscall_kstack]
    push qword [rel g_saved_user_rsp]
    push rcx
    push r11
    push rdi
    push rsi
    push rdx
    push r8
    push r9
    push r10
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov rcx, rdx           ; a3
    mov rdx, rsi           ; a2
    mov rsi, rdi           ; a1
    mov rdi, rax           ; nr
    mov r8, rsp            ; SyscallFrame*
    sub rsp, 8             ; keep 16-byte alignment for the call
    sti
    call syscall_dispatch
    cli
    add rsp, 8
syscall_return:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rsi
    pop rdi
    pop r11
    pop rcx
    pop rsp                ; back to the user stack
    o64 sysret

; void return_to_user(SyscallFrame* frame): resume user mode from a saved frame with rax = 0 (fork child).
global return_to_user
return_to_user:
    cli
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov rsp, rdi
    xor eax, eax
    jmp syscall_return

section .note.GNU-stack noalloc noexec nowrite progbits
