// Own GDT: null, kernel code/data, user data/code (sysret-friendly order), TSS.
#include "gdt.hpp"
#include <stdint.h>

namespace {
struct Entry { uint16_t limit; uint16_t base_lo; uint8_t base_mid, access, gran, base_hi; } __attribute__((packed));
struct Tss {
    uint32_t r0; uint64_t rsp[3]; uint64_t r1; uint64_t ist[7]; uint64_t r2; uint16_t r3, iopb;
} __attribute__((packed));
struct TssDesc { uint16_t limit, base_lo; uint8_t base_mid, access, gran, base_hi; uint32_t base_up, r; } __attribute__((packed));
struct Gdtr { uint16_t size; uint64_t base; } __attribute__((packed));

struct Table { Entry e[5]; TssDesc tss; } __attribute__((packed, aligned(16)));

Table table;
Tss tss;
alignas(16) uint8_t ist1_stack[16384];   // for double faults
}

extern "C" void gdt_load(Gdtr*);  // entry.asm

static Entry make(uint8_t access, uint8_t gran) { return {0, 0, 0, access, gran, 0}; }

void gdt_init() {
    table.e[0] = make(0, 0);          // null
    table.e[1] = make(0x9A, 0x20);    // 0x08 kernel code (64-bit)
    table.e[2] = make(0x92, 0x00);    // 0x10 kernel data
    table.e[3] = make(0xF2, 0x00);    // 0x18 user data
    table.e[4] = make(0xFA, 0x20);    // 0x20 user code

    tss.ist[0] = reinterpret_cast<uint64_t>(ist1_stack + sizeof ist1_stack);
    tss.iopb = sizeof(Tss);
    uint64_t b = reinterpret_cast<uint64_t>(&tss);
    table.tss = {sizeof(Tss) - 1, static_cast<uint16_t>(b), static_cast<uint8_t>(b >> 16),
                 0x89, 0, static_cast<uint8_t>(b >> 24), static_cast<uint32_t>(b >> 32), 0};

    Gdtr g{sizeof(Table) - 1, reinterpret_cast<uint64_t>(&table)};
    gdt_load(&g);
}
