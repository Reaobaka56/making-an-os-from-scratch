// Our own 4-level page tables (replaces the ones Limine set up), plus per-process address spaces.
#include "vmm.hpp"
#include "pmm.hpp"
#include "mm.hpp"
#include "irq.hpp"
#include "io.hpp"
#include "console.hpp"
#include <string.h>

extern "C" char __text_start[], __text_end[], __rodata_start[], __rodata_end[], __data_start[], __data_end[];

namespace {
constexpr uint64_t HUGE = 1ULL << 7;
constexpr uint64_t ADDR = 0x000FFFFFFFFFF000ULL;
constexpr uint64_t MiB2 = 2ULL << 20;
constexpr uint64_t MMIO_BASE = 0xffffc00000000000ULL;

uint64_t kroot;
uint64_t mmio_next = MMIO_BASE;

uint64_t* tbl(uint64_t phys) { return static_cast<uint64_t*>(phys_to_virt(phys)); }

// Next-level table, optionally created. nullptr if absent or a huge page is in the way.
uint64_t* next(uint64_t* t, uint64_t idx, bool create, bool user) {
    if (!(t[idx] & VMM_PRESENT)) {
        if (!create) return nullptr;
        uint64_t p = pmm_alloc();
        if (!p) return nullptr;
        memset(tbl(p), 0, PAGE_SIZE);
        t[idx] = p | VMM_PRESENT | VMM_WRITE | (user ? VMM_USER : 0);
    }
    if (t[idx] & HUGE) return nullptr;
    return tbl(t[idx] & ADDR);
}

uint64_t* pte_for(uint64_t root, uint64_t virt, bool create) {
    bool user = virt < USER_TOP;
    uint64_t* t = tbl(root);
    t = next(t, (virt >> 39) & 511, create, user); if (!t) return nullptr;
    t = next(t, (virt >> 30) & 511, create, user); if (!t) return nullptr;
    t = next(t, (virt >> 21) & 511, create, user); if (!t) return nullptr;
    return &t[(virt >> 12) & 511];
}

void map_2m(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t* t = tbl(kroot);
    t = next(t, (virt >> 39) & 511, true, false);
    t = next(t, (virt >> 30) & 511, true, false);
    t[(virt >> 21) & 511] = phys | flags | VMM_PRESENT | HUGE;
}

// Walks root for virt. On success returns the leaf entry and the physical address.
// 'and_flags' accumulates the permission bits of all levels (for user-access checks).
bool lookup(uint64_t root, uint64_t virt, uint64_t* leaf, uint64_t* phys, uint64_t* and_flags = nullptr) {
    uint64_t* t = tbl(root);
    uint64_t acc = VMM_USER | VMM_WRITE;
    uint64_t e = t[(virt >> 39) & 511]; if (!(e & VMM_PRESENT)) return false;
    acc &= e; t = tbl(e & ADDR);
    e = t[(virt >> 30) & 511]; if (!(e & VMM_PRESENT)) return false;
    acc &= e;
    if (e & HUGE) { if (phys) *phys = (e & 0x000FFFFFC0000000ULL) | (virt & 0x3FFFFFFFULL); goto done; }
    t = tbl(e & ADDR);
    e = t[(virt >> 21) & 511]; if (!(e & VMM_PRESENT)) return false;
    acc &= e;
    if (e & HUGE) { if (phys) *phys = (e & 0x000FFFFFFFE00000ULL) | (virt & (MiB2 - 1)); goto done; }
    t = tbl(e & ADDR);
    e = t[(virt >> 12) & 511]; if (!(e & VMM_PRESENT)) return false;
    acc &= e;
    if (phys) *phys = (e & ADDR) | (virt & 0xFFF);
done:
    if (leaf) *leaf = e;
    if (and_flags) *and_flags = acc;
    return true;
}

void free_level(uint64_t table_phys, int level, bool user_half_only) {
    uint64_t* t = tbl(table_phys);
    for (int i = 0; i < (user_half_only ? 256 : 512); i++) {
        if (!(t[i] & VMM_PRESENT)) continue;
        if (level == 1) pmm_free(t[i] & ADDR);              // leaf: owned user frame
        else if (!(t[i] & HUGE)) free_level(t[i] & ADDR, level - 1, false);
    }
    pmm_free(table_phys);
}
}  // namespace

void vmm_init(LimineMemmapResponse* mm, LimineExecAddrResponse* exec) {
    wrmsr(0xC0000080, rdmsr(0xC0000080) | (1ULL << 11));  // EFER.NXE

    kroot = pmm_alloc();
    memset(tbl(kroot), 0, PAGE_SIZE);

    // Pre-create every upper-half PML4 slot so address spaces cloned later share all kernel mappings.
    for (int i = 256; i < 512; i++) next(tbl(kroot), i, true, false);

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
            vmm_map_in(kroot, v, exec->physical_base + (v - exec->virtual_base), flags);
    };
    map_range(__text_start,   __text_end,   0);
    map_range(__rodata_start, __rodata_end, VMM_NX);
    map_range(__data_start,   __data_end,   VMM_WRITE | VMM_NX);

    asm volatile("mov %0, %%cr3" :: "r"(kroot) : "memory");
}

uint64_t vmm_kernel_root() { return kroot; }
uint64_t vmm_current_root() { uint64_t r; asm volatile("mov %%cr3, %0" : "=r"(r)); return r & ADDR; }
void vmm_switch(uint64_t root) { asm volatile("mov %0, %%cr3" :: "r"(root) : "memory"); }

uint64_t vmm_create_space() {
    IrqGuard guard;
    uint64_t root = pmm_alloc();
    if (!root) return 0;
    memset(tbl(root), 0, PAGE_SIZE);
    for (int i = 256; i < 512; i++) tbl(root)[i] = tbl(kroot)[i];
    return root;
}

void vmm_destroy_space(uint64_t root) {
    IrqGuard guard;
    if (root == kroot) return;
    free_level(root, 4, true);
}

bool vmm_map_in(uint64_t root, uint64_t virt, uint64_t phys, uint64_t flags) {
    IrqGuard guard;
    uint64_t* pte = pte_for(root, virt, true);
    if (!pte) return false;
    *pte = (phys & ADDR) | flags | VMM_PRESENT;
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
    return true;
}

bool vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) { return vmm_map_in(kroot, virt, phys, flags); }

bool vmm_unmap(uint64_t virt) {
    IrqGuard guard;
    uint64_t* pte = pte_for(kroot, virt, false);
    if (!pte || !(*pte & VMM_PRESENT)) return false;
    *pte = 0;
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
    return true;
}

bool vmm_translate_in(uint64_t root, uint64_t virt, uint64_t* phys) { return lookup(root, virt, nullptr, phys); }
bool vmm_translate(uint64_t virt, uint64_t* phys) { return lookup(kroot, virt, nullptr, phys); }

bool vmm_user_ok(uint64_t virt, uint64_t len, bool write) {
    if (virt >= USER_TOP || len > USER_TOP || virt + len > USER_TOP) return false;
    if (len == 0) return true;
    uint64_t root = vmm_current_root();
    for (uint64_t p = virt & ~(PAGE_SIZE - 1); p < virt + len; p += PAGE_SIZE) {
        uint64_t leaf, acc;
        if (!lookup(root, p, &leaf, nullptr, &acc)) return false;
        if (!(leaf & VMM_USER) || !(acc & VMM_USER)) return false;
        if (write && (!(leaf & VMM_WRITE) || !(acc & VMM_WRITE))) return false;
    }
    return true;
}

void* mmio_map(uint64_t phys, uint64_t bytes) {
    uint64_t off = phys & (PAGE_SIZE - 1);
    uint64_t base = phys - off;
    uint64_t pages = (bytes + off + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t v = mmio_next;
    for (uint64_t i = 0; i < pages; i++)
        vmm_map(v + i * PAGE_SIZE, base + i * PAGE_SIZE, VMM_WRITE | VMM_NX | VMM_PCD | VMM_PWT);
    mmio_next += pages * PAGE_SIZE;
    return reinterpret_cast<void*>(v + off);
}
