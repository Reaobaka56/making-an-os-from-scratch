// Our own 4-level page tables (replaces the ones Limine set up).
#include "vmm.hpp"
#include "pmm.hpp"
#include "mm.hpp"
#include "console.hpp"
#include <string.h>

extern "C" char __text_start[], __text_end[], __rodata_start[], __rodata_end[], __data_start[], __data_end[];

namespace {
constexpr uint64_t HUGE = 1ULL << 7;
constexpr uint64_t ADDR = 0x000FFFFFFFFFF000ULL;
constexpr uint64_t MiB2 = 2ULL << 20;

uint64_t pml4_phys;

uint64_t* tbl(uint64_t phys) { return static_cast<uint64_t*>(phys_to_virt(phys)); }

// Returns the next-level table, creating it if asked. nullptr if absent or a huge page is in the way.
uint64_t* next(uint64_t* t, uint64_t idx, bool create) {
    if (!(t[idx] & VMM_PRESENT)) {
        if (!create) return nullptr;
        uint64_t p = pmm_alloc();
        if (!p) return nullptr;
        memset(tbl(p), 0, PAGE_SIZE);
        t[idx] = p | VMM_PRESENT | VMM_WRITE;
    }
    if (t[idx] & HUGE) return nullptr;
    return tbl(t[idx] & ADDR);
}

uint64_t* pte_for(uint64_t virt, bool create) {
    uint64_t* t = tbl(pml4_phys);
    t = next(t, (virt >> 39) & 511, create); if (!t) return nullptr;
    t = next(t, (virt >> 30) & 511, create); if (!t) return nullptr;
    t = next(t, (virt >> 21) & 511, create); if (!t) return nullptr;
    return &t[(virt >> 12) & 511];
}

void map_2m(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t* t = tbl(pml4_phys);
    t = next(t, (virt >> 39) & 511, true);
    t = next(t, (virt >> 30) & 511, true);
    t[(virt >> 21) & 511] = phys | flags | VMM_PRESENT | HUGE;
}
}  // namespace

void vmm_init(LimineMemmapResponse* mm, LimineExecAddrResponse* exec) {
    // Make sure the NX bit is honoured.
    uint32_t lo, hi;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000080));
    lo |= 1u << 11;
    asm volatile("wrmsr" :: "a"(lo), "d"(hi), "c"(0xC0000080));

    pml4_phys = pmm_alloc();
    memset(tbl(pml4_phys), 0, PAGE_SIZE);

    // Direct map of physical memory (2 MiB pages, RW, no-exec), per memory-map entry.
    for (uint64_t i = 0; i < mm->entry_count; i++) {
        auto* e = mm->entries[i];
        if (e->type == LIMINE_MEMMAP_BAD_MEMORY) continue;
        uint64_t start = e->base & ~(MiB2 - 1);
        uint64_t end = (e->base + e->length + MiB2 - 1) & ~(MiB2 - 1);
        for (uint64_t p = start; p < end; p += MiB2) map_2m(g_hhdm + p, p, VMM_WRITE | VMM_NX);
    }

    // Kernel image with W^X permissions.
    auto map_range = [&](char* a, char* b, uint64_t flags) {
        for (uint64_t v = reinterpret_cast<uint64_t>(a); v < reinterpret_cast<uint64_t>(b); v += PAGE_SIZE)
            vmm_map(v, exec->physical_base + (v - exec->virtual_base), flags);
    };
    map_range(__text_start,   __text_end,   0);
    map_range(__rodata_start, __rodata_end, VMM_NX);
    map_range(__data_start,   __data_end,   VMM_WRITE | VMM_NX);

    asm volatile("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
}

bool vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t* pte = pte_for(virt, true);
    if (!pte) return false;
    *pte = (phys & ADDR) | flags | VMM_PRESENT;
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
    return true;
}

bool vmm_unmap(uint64_t virt) {
    uint64_t* pte = pte_for(virt, false);
    if (!pte || !(*pte & VMM_PRESENT)) return false;
    *pte = 0;
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
    return true;
}

bool vmm_translate(uint64_t virt, uint64_t* phys) {
    uint64_t* t = tbl(pml4_phys);
    uint64_t e = t[(virt >> 39) & 511]; if (!(e & VMM_PRESENT)) return false;
    t = tbl(e & ADDR);
    e = t[(virt >> 30) & 511]; if (!(e & VMM_PRESENT)) return false;
    if (e & HUGE) { *phys = (e & 0x000FFFFFC0000000ULL) | (virt & 0x3FFFFFFFULL); return true; }
    t = tbl(e & ADDR);
    e = t[(virt >> 21) & 511]; if (!(e & VMM_PRESENT)) return false;
    if (e & HUGE) { *phys = (e & 0x000FFFFFFFE00000ULL) | (virt & (MiB2 - 1)); return true; }
    t = tbl(e & ADDR);
    e = t[(virt >> 12) & 511]; if (!(e & VMM_PRESENT)) return false;
    *phys = (e & ADDR) | (virt & 0xFFF);
    return true;
}
