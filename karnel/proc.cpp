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
constexpr size_t   ARGS_MAX = 3072;     // bytes of argument strings

// Lays out argc/argv/envp on the top page of the user stack: [rsp]=argc, argv[], NULL, envp NULL.
bool build_stack(uint64_t root, int argc, char** argv, uint64_t* rsp_out) {
    static_assert(ARGS_MAX < PAGE_SIZE, "args must fit one page");
    uint8_t* page = static_cast<uint8_t*>(kmalloc(PAGE_SIZE));
    if (!page) return false;
    memset(page, 0, PAGE_SIZE);
    const uint64_t base = STACK_TOP - PAGE_SIZE;           // user address of page[0]

    size_t p = PAGE_SIZE;
    uint64_t ptrs[17];
    if (argc > 16) argc = 16;
    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(argv[i]) + 1;
        if (len > p || PAGE_SIZE - (p - len) > ARGS_MAX) { kfree(page); return false; }
        p -= len;
        memcpy(page + p, argv[i], len);
        ptrs[i] = base + p;
    }
    p &= ~size_t(15);
    size_t words = 1 + argc + 1 + 1;                       // argc, argv..., NULL, envp NULL
    size_t sp = (p - words * 8) & ~size_t(15);
    auto* w = reinterpret_cast<uint64_t*>(page + sp);
    w[0] = argc;
    for (int i = 0; i < argc; i++) w[1 + i] = ptrs[i];
    w[1 + argc] = 0;
    w[2 + argc] = 0;

    uint64_t frame;
    bool ok = vmm_translate_in(root, base, &frame);
    if (ok) memcpy(phys_to_virt(frame & ~(PAGE_SIZE - 1)), page, PAGE_SIZE);
    kfree(page);
    *rsp_out = base + sp;
    return ok;
}
}  // namespace

int proc_prepare(const char* path, int argc, char** argv, ProcImage* out) {
    int idx = fs_find(path);
    if (idx < 0) return -1;
    uint32_t size = fs_size(idx);
    auto* image = static_cast<uint8_t*>(kmalloc(size));
    if (!image) return -2;
    if (fs_read(idx, 0, image, size) != size) { kfree(image); return -3; }

    uint64_t root = vmm_create_space();
    if (!root) { kfree(image); return -2; }

    uint64_t entry = 0, end = 0;
    bool ok = elf_load(root, image, size, &entry, &end);
    kfree(image);
    for (uint64_t i = 0; ok && i < STACK_PAGES; i++) {
        uint64_t f = pmm_alloc();
        if (!f) { ok = false; break; }
        memset(phys_to_virt(f), 0, PAGE_SIZE);
        ok = vmm_map_in(root, STACK_TOP - (i + 1) * PAGE_SIZE, f, VMM_USER | VMM_WRITE | VMM_NX);
    }
    uint64_t rsp = 0;
    if (ok) {
        char* dflt[1] = {const_cast<char*>(path)};
        ok = build_stack(root, argc ? argc : 1, argc ? argv : dflt, &rsp);
    }
    if (!ok) { vmm_destroy_space(root); return -4; }

    out->root = root; out->entry = entry; out->rsp = rsp;
    out->brk = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    return 0;
}

int proc_spawn(const char* path, int argc, char** argv) {
    ProcImage img;
    int r = proc_prepare(path, argc, argv, &img);
    if (r < 0) return r;
    const char* base = path;
    for (const char* c = path; *c; c++) if (*c == '/') base = c + 1;
    Thread* t = thread_create_user(base, img.root, img.entry, img.rsp, img.brk, thread_current()->id);
    return static_cast<int>(t->id);
}

int proc_wait(int pid, int* status) {
    Thread* me = thread_current();
    for (;;) {
        int r = sched_collect(me->id, pid, status);
        if (r != 0) return r;
        sleep_ms(10);
    }
}
