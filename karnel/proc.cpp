#include "proc.hpp"
#include "fs.hpp"
#include "elf.hpp"
#include "vmm.hpp"
#include "pmm.hpp"
#include "mm.hpp"
#include "heap.hpp"
#include "sched.hpp"
#include "console.hpp"
#include <string.h>

namespace {
constexpr uint64_t STACK_TOP = 0x00007ffffffff000ULL;
constexpr uint64_t STACK_PAGES = 16;
}

int proc_spawn(const char* path) {
    int idx = fs_find(path);
    if (idx < 0) return -1;
    uint32_t size = fs_size(idx);
    auto* image = static_cast<uint8_t*>(kmalloc(size));
    if (!image) return -2;
    if (fs_read(idx, 0, image, size) != size) { kfree(image); return -3; }

    uint64_t root = vmm_create_space();
    if (!root) { kfree(image); return -2; }

    uint64_t entry = 0;
    bool ok = elf_load(root, image, size, &entry);
    kfree(image);
    for (uint64_t i = 0; ok && i < STACK_PAGES; i++) {
        uint64_t f = pmm_alloc();
        if (!f) { ok = false; break; }
        memset(phys_to_virt(f), 0, PAGE_SIZE);
        ok = vmm_map_in(root, STACK_TOP - (i + 1) * PAGE_SIZE, f, VMM_USER | VMM_WRITE | VMM_NX);
    }
    if (!ok) { vmm_destroy_space(root); return -4; }

    const char* base = path;
    for (const char* c = path; *c; c++) if (*c == '/') base = c + 1;
    Thread* t = thread_create_user(base, root, entry, STACK_TOP);
    return static_cast<int>(t->id);
}
