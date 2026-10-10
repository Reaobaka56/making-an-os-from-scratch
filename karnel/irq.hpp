#pragma once
#include <stdint.h>

// RAII: disable interrupts for a scope, restore previous state after.
struct IrqGuard {
    uint64_t flags;
    IrqGuard() { asm volatile("pushfq; pop %0; cli" : "=r"(flags) :: "memory"); }
    ~IrqGuard() { if (flags & 0x200) asm volatile("sti" ::: "memory"); }
    IrqGuard(const IrqGuard&) = delete;
    IrqGuard& operator=(const IrqGuard&) = delete;
};
