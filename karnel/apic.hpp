#pragma once
#include <stdint.h>

constexpr int VEC_TIMER = 0x20;
constexpr int VEC_KBD   = 0x21;
constexpr int VEC_SPURIOUS = 0xFF;
constexpr uint64_t TIMER_HZ = 100;

void     apic_init();                 // disables PIC, enables LAPIC + IOAPIC, calibrates and starts the timer
void     lapic_eoi();
void     ioapic_route(uint8_t gsi, uint8_t vector);
uint64_t timer_ticks();
inline uint64_t uptime_ms() { return timer_ticks() * (1000 / TIMER_HZ); }
