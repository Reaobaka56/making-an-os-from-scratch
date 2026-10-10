// Local APIC (timer), IOAPIC (keyboard routing), legacy PIC disabled.
#include "apic.hpp"
#include "idt.hpp"
#include "io.hpp"
#include "vmm.hpp"
#include "console.hpp"
#include "sched.hpp"

namespace {
volatile uint32_t* lapic;
volatile uint32_t* ioapic;
volatile uint64_t ticks;

constexpr uint32_t L_ID = 0x20, L_EOI = 0xB0, L_SVR = 0xF0, L_TIMER = 0x320,
                   L_INIT = 0x380, L_CUR = 0x390, L_DIV = 0x3E0;

inline uint32_t lr(uint32_t off) { return lapic[off / 4]; }
inline void lw(uint32_t off, uint32_t v) { lapic[off / 4] = v; }

void io_write(uint8_t reg, uint32_t v) { ioapic[0] = reg; ioapic[4] = v; }

void disable_pic() {
    outb(0x20, 0x11); outb(0xA0, 0x11);   // init
    outb(0x21, 0xE0); outb(0xA1, 0xE8);   // remap out of the way (0xE0-0xEF)
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFF); outb(0xA1, 0xFF);   // mask everything
}

// Measure LAPIC timer ticks per millisecond against PIT channel 2 (10 ms one-shot).
uint32_t calibrate() {
    outb(0x61, (inb(0x61) & ~0x02) | 0x01);       // gate on, speaker off
    outb(0x43, 0xB0);                              // ch2, lo/hi, mode 0
    outb(0x42, 11932 & 0xFF); outb(0x42, 11932 >> 8);
    lw(L_DIV, 0x3);                                // divide by 16
    lw(L_TIMER, 0x10000);                          // masked
    lw(L_INIT, 0xFFFFFFFF);
    while (!(inb(0x61) & 0x20)) {}
    uint32_t left = lr(L_CUR);
    lw(L_INIT, 0);
    return (0xFFFFFFFFu - left) / 10;
}

void timer_irq(Frame*) { ticks = ticks + 1; sched_tick(); }
}  // namespace

void apic_init() {
    disable_pic();

    uint64_t base = rdmsr(0x1B);
    wrmsr(0x1B, base | (1ULL << 11));              // global enable
    lapic = static_cast<volatile uint32_t*>(mmio_map(base & 0xFFFFF000ULL, 0x1000));
    ioapic = static_cast<volatile uint32_t*>(mmio_map(0xFEC00000ULL, 0x1000));

    lw(L_SVR, 0x100 | VEC_SPURIOUS);               // software enable
    uint32_t per_ms = calibrate();
    kprintf("apic: id=%u, timer %u ticks/ms\n", lr(L_ID) >> 24, per_ms);

    idt_set_handler(VEC_TIMER, timer_irq);
    lw(L_DIV, 0x3);
    lw(L_TIMER, VEC_TIMER | (1u << 17));           // periodic
    lw(L_INIT, per_ms * (1000 / TIMER_HZ));
}

void lapic_eoi() { lw(L_EOI, 0); }

void ioapic_route(uint8_t gsi, uint8_t vector) {
    uint32_t dest = lr(L_ID) >> 24;
    io_write(0x11 + 2 * gsi, dest << 24);
    io_write(0x10 + 2 * gsi, vector);              // fixed, physical, edge, active-high, unmasked
}

uint64_t timer_ticks() { return ticks; }
