#pragma once
#include <stdint.h>

extern uint64_t g_hhdm;  // higher-half direct map offset

inline void* phys_to_virt(uint64_t phys) { return reinterpret_cast<void*>(g_hhdm + phys); }

void mm_init();       // pmm + vmm + heap
void mm_selftest();   // prints PASS/FAIL lines
