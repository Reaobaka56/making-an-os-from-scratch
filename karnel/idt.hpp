#pragma once
#include <stdint.h>
struct Frame {  // matches push order in isr_common
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector, error;
    uint64_t rip, cs, rflags, rsp, ss;
};
void idt_init();
