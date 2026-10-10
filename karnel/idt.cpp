#include "idt.hpp"
#include "console.hpp"

namespace {
struct Gate { uint16_t lo, sel; uint8_t ist, attr; uint16_t mid; uint32_t hi, r; } __attribute__((packed));
struct Idtr { uint16_t size; uint64_t base; } __attribute__((packed));
Gate idt[256];

const char* names[32] = {
    "Divide error", "Debug", "NMI", "Breakpoint", "Overflow", "Bound range", "Invalid opcode",
    "Device not available", "Double fault", "Coprocessor overrun", "Invalid TSS", "Segment not present",
    "Stack fault", "General protection fault", "Page fault", "Reserved", "x87 FP", "Alignment check",
    "Machine check", "SIMD FP", "Virtualization", "Control protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Hypervisor injection", "VMM comm", "Security", "Reserved"};
}

extern "C" uint64_t isr_table[256];

extern "C" void isr_handler(Frame* f) {
    kprintf("\n*** EXCEPTION %lu: %s ***\n", f->vector, f->vector < 32 ? names[f->vector] : "?");
    kprintf("err=%lx rip=%lx cs=%lx rflags=%lx rsp=%lx\n", f->error, f->rip, f->cs, f->rflags, f->rsp);
    kprintf("rax=%lx rbx=%lx rcx=%lx rdx=%lx\nrsi=%lx rdi=%lx rbp=%lx\n",
            f->rax, f->rbx, f->rcx, f->rdx, f->rsi, f->rdi, f->rbp);
    if (f->vector == 14) {
        uint64_t cr2; asm volatile("mov %%cr2, %0" : "=r"(cr2));
        kprintf("cr2=%lx\n", cr2);
    }
    for (;;) asm volatile("cli; hlt");
}

void idt_init() {
    for (int i = 0; i < 256; i++) {
        uint64_t a = isr_table[i];
        idt[i] = {static_cast<uint16_t>(a), 0x08, static_cast<uint8_t>(i == 8 ? 1 : 0), 0x8E,
                  static_cast<uint16_t>(a >> 16), static_cast<uint32_t>(a >> 32), 0};
    }
    Idtr r{sizeof idt - 1, reinterpret_cast<uint64_t>(&idt)};
    asm volatile("lidt %0" :: "m"(r));
}
