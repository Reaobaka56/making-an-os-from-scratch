// Physical frame allocator: one bit per 4 KiB frame (1 = used).
#include "pmm.hpp"
#include "mm.hpp"
#include "console.hpp"
#include "irq.hpp"
#include <string.h>

static uint64_t* bm;
static uint64_t g_frames, g_qwords, g_free, g_total, g_hint;

static inline bool test(uint64_t i) { return bm[i / 64] >> (i % 64) & 1; }
static inline void set(uint64_t i)  { bm[i / 64] |= 1ULL << (i % 64); }
static inline void clr(uint64_t i)  { bm[i / 64] &= ~(1ULL << (i % 64)); }

void pmm_init(LimineMemmapResponse* mm) {
    uint64_t top = 0;
    for (uint64_t i = 0; i < mm->entry_count; i++) {
        auto* e = mm->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->base + e->length > top) top = e->base + e->length;
    }
    g_frames = top / PAGE_SIZE;
    g_qwords = (g_frames + 63) / 64;
    uint64_t bytes = g_qwords * 8;
    uint64_t need = (bytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Host the bitmap inside a usable region above 1 MiB.
    uint64_t bm_phys = 0;
    for (uint64_t i = 0; i < mm->entry_count; i++) {
        auto* e = mm->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->base >= 0x100000 && e->length >= need) { bm_phys = e->base; break; }
    }
    if (!bm_phys) { kprintf("pmm: no region for bitmap\n"); for (;;) asm volatile("cli; hlt"); }
    bm = static_cast<uint64_t*>(phys_to_virt(bm_phys));
    memset(bm, 0xFF, bytes);  // everything used (also pads the last qword)

    for (uint64_t i = 0; i < mm->entry_count; i++) {
        auto* e = mm->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE) continue;
        for (uint64_t f = e->base / PAGE_SIZE; f < (e->base + e->length) / PAGE_SIZE; f++) { clr(f); g_free++; }
    }
    // Reserve frame 0 and the bitmap itself.
    if (!test(0)) { set(0); g_free--; }
    for (uint64_t f = bm_phys / PAGE_SIZE; f < (bm_phys + need) / PAGE_SIZE; f++) if (!test(f)) { set(f); g_free--; }
    g_total = g_free;
}

uint64_t pmm_alloc() {
    IrqGuard guard;
    for (int pass = 0; pass < 2; pass++) {
        for (uint64_t i = g_hint; i < g_qwords; i++) {
            if (bm[i] == ~0ULL) continue;
            uint64_t f = i * 64 + __builtin_ctzll(~bm[i]);
            set(f); g_free--; g_hint = i;
            return f * PAGE_SIZE;
        }
        g_hint = 0;
    }
    return 0;
}

void pmm_free(uint64_t phys) {
    IrqGuard guard;
    uint64_t f = phys / PAGE_SIZE;
    if (f >= g_frames || !test(f)) { kprintf("pmm: bad/double free %lx\n", phys); return; }
    clr(f); g_free++;
    if (f / 64 < g_hint) g_hint = f / 64;
}

uint64_t pmm_free_frames()  { return g_free; }
uint64_t pmm_total_frames() { return g_total; }
