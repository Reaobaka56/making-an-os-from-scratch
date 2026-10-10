#include "mm.hpp"
#include "limine.hpp"
#include "pmm.hpp"
#include "vmm.hpp"
#include "heap.hpp"
#include "console.hpp"

LIMINE_REQ volatile LimineMemmapRequest memmap_request = {
    {LIMINE_MAGIC0, LIMINE_MAGIC1, 0x67cf3d9d378a806fULL, 0xe304acdfc50c3c62ULL}, 0, nullptr};
LIMINE_REQ volatile LimineHhdmRequest hhdm_request = {
    {LIMINE_MAGIC0, LIMINE_MAGIC1, 0x48dcf1cb8ad2b852ULL, 0x63984e959a98244bULL}, 0, nullptr};
LIMINE_REQ volatile LimineExecAddrRequest exec_request = {
    {LIMINE_MAGIC0, LIMINE_MAGIC1, 0x71ba76863cc55f63ULL, 0xb2644a48c516a487ULL}, 0, nullptr};

uint64_t g_hhdm;

[[noreturn]] static void die(const char* m) { kprintf("mm: %s\n", m); for (;;) asm volatile("cli; hlt"); }

void mm_init() {
    auto* mm = memmap_request.response;
    auto* hh = hhdm_request.response;
    auto* ex = exec_request.response;
    if (!mm || !hh || !ex) die("missing Limine response");
    g_hhdm = hh->offset;

    static const char* names[] = {"usable", "reserved", "acpi-reclaim", "acpi-nvs", "bad",
                                  "bootloader", "kernel", "framebuffer"};
    kprintf("hhdm=%lx\nmemory map:\n", g_hhdm);
    for (uint64_t i = 0; i < mm->entry_count; i++) {
        auto* e = mm->entries[i];
        kprintf("  %lx - %lx  %s\n", e->base, e->base + e->length, e->type < 8 ? names[e->type] : "?");
    }

    pmm_init(mm);
    kprintf("pmm: %lu frames usable (%lu MiB)\n", pmm_total_frames(), pmm_total_frames() * 4 / 1024);
    vmm_init(mm, ex);
    kprintf("vmm: own page tables loaded\n");
    heap_init();
    kprintf("heap: ready (%lu KiB mapped)\n", heap_bytes_mapped() / 1024);
}

static void check(const char* name, bool ok) { kprintf("  [%s] %s\n", ok ? "PASS" : "FAIL", name); }

void mm_selftest() {
    kprintf("self-test:\n");

    uint64_t before = pmm_free_frames();
    uint64_t a = pmm_alloc(), b = pmm_alloc();
    check("pmm alloc distinct, non-zero", a && b && a != b);
    pmm_free(a); pmm_free(b);
    check("pmm free restores count", pmm_free_frames() == before);

    constexpr uint64_t V = 0xffffb00000000000ULL;
    uint64_t f = pmm_alloc(), t = 0;
    bool mapped = vmm_map(V, f, VMM_WRITE | VMM_NX);
    *reinterpret_cast<volatile uint64_t*>(V) = 0x1122334455667788ULL;
    check("vmm map + write/read", mapped && *reinterpret_cast<volatile uint64_t*>(V) == 0x1122334455667788ULL);
    check("vmm translate", vmm_translate(V, &t) && t == f);
    check("hhdm alias sees same data", *static_cast<volatile uint64_t*>(phys_to_virt(f)) == 0x1122334455667788ULL);
    check("vmm unmap", vmm_unmap(V) && !vmm_translate(V, &t));
    pmm_free(f);

    void* p = kmalloc(100);
    void* q = kmalloc(5000);
    check("kmalloc distinct", p && q && p != q);
    auto* big = new uint8_t[1 << 20];  // 1 MiB forces the heap to grow
    for (size_t i = 0; i < (1 << 20); i += 4096) big[i] = static_cast<uint8_t>(i >> 12);
    bool ok = true;
    for (size_t i = 0; i < (1 << 20); i += 4096) ok &= big[i] == static_cast<uint8_t>(i >> 12);
    check("new[] 1 MiB write/read", ok);
    kprintf("  heap mapped: %lu KiB\n", heap_bytes_mapped() / 1024);
    delete[] big; kfree(p); kfree(q);
    check("heap fully coalesced after free", heap_free_blocks() == 1);
}
