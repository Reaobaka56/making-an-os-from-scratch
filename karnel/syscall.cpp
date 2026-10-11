#include "syscall.hpp"
#include "sched.hpp"
#include "vmm.hpp"
#include "pmm.hpp"
#include "mm.hpp"
#include "fs.hpp"
#include "heap.hpp"
#include "irq.hpp"
#include "fdtable.hpp"
#include "proc.hpp"
#include "kbd.hpp"
#include "serial.hpp"
#include "apic.hpp"
#include "console.hpp"
#include "io.hpp"
#include <string.h>

extern "C" void syscall_entry();

void syscall_init() {
    wrmsr(0xC0000080, rdmsr(0xC0000080) | 1);                       // EFER.SCE
    wrmsr(0xC0000081, (0x10ULL << 48) | (0x08ULL << 32));            // STAR: sysret base 0x10, syscall CS 0x08
    wrmsr(0xC0000082, reinterpret_cast<uint64_t>(syscall_entry));    // LSTAR
    wrmsr(0xC0000084, 0x200 | 0x100 | 0x400);                        // SFMASK: clear IF, TF, DF
}

int input_getc() {
    for (;;) {
        int c = kbd_pop();
        if (c >= 0) return c;
        c = serial_getc_nb();
        if (c >= 0) return c == '\r' ? '\n' : c;
        sleep_ms(5);
    }
}

namespace {
constexpr uint64_t MMAP_BASE = 0x0000600000000000ULL;
constexpr uint64_t MMAP_LIMIT = 0x00007f0000000000ULL;
constexpr uint64_t BRK_MAX = 256ULL << 20;
constexpr int ARGV_MAX = 16, ARG_LEN = 128;

bool copy_user_str(char* dst, uint64_t src, size_t max) {
    for (size_t i = 0; i < max; i++) {
        if (!vmm_user_ok(src + i, 1, false)) return false;
        dst[i] = *reinterpret_cast<const char*>(src + i);
        if (!dst[i]) return true;
    }
    return false;
}

// Copies a NULL-terminated user argv into kernel memory. Returns argc or -1.
int copy_user_argv(uint64_t uargv, char (*store)[ARG_LEN], char** out) {
    if (!uargv) return 0;
    int n = 0;
    for (; n < ARGV_MAX; n++) {
        if (!vmm_user_ok(uargv + n * 8, 8, false)) return -1;
        uint64_t p = *reinterpret_cast<const uint64_t*>(uargv + n * 8);
        if (!p) break;
        if (!copy_user_str(store[n], p, ARG_LEN)) return -1;
        out[n] = store[n];
    }
    return n;
}

const char* base_name(const char* path) {
    const char* b = path;
    for (const char* c = path; *c; c++) if (*c == '/') b = c + 1;
    return b;
}

bool map_zeroed(uint64_t root, uint64_t virt, uint64_t flags) {
    uint64_t f = pmm_alloc();
    if (!f) return false;
    memset(phys_to_virt(f), 0, PAGE_SIZE);
    if (!vmm_map_in(root, virt, f, flags)) { pmm_free(f); return false; }
    return true;
}

void free_range(uint64_t root, uint64_t from, uint64_t to) {
    for (uint64_t v = from; v < to; v += PAGE_SIZE) {
        uint64_t phys;
        if (vmm_unmap_in(root, v, &phys)) pmm_free(phys);
    }
}

int64_t sys_open(Thread* t, uint64_t upath, uint64_t flags) {
    char path[48];
    if (!copy_user_str(path, upath, sizeof path)) return -1;
    int fd = fd_alloc(t);
    if (fd < 0) return -3;
    OpenFile& f = t->files[fd];
    memset(&f, 0, sizeof f);
    if (flags & 1) {
        f.wfile = wfile_create(path);
        if (!f.wfile) return -4;
        f.kind = FD_WFILE;
    } else {
        int idx = fs_find(path);
        if (idx < 0) return -2;
        f.kind = FD_FILE;
        f.index = idx;
    }
    return fd;
}
}  // namespace

extern "C" int64_t syscall_dispatch(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3, SyscallFrame* frame) {
    Thread* t = thread_current();
    switch (nr) {
    case SYS_WRITE: {
        if (!vmm_user_ok(a2, a3, false)) return -1;
        if (a1 >= MAX_FDS) return -1;
        OpenFile& f = t->files[a1];
        auto* p = reinterpret_cast<const char*>(a2);
        switch (f.kind) {
        case FD_CONSOLE_OUT: for (uint64_t i = 0; i < a3; i++) kputc(p[i]); return a3;
        case FD_PIPE_W:      return pipe_write(f.pipe, p, a3);
        case FD_WFILE:       return wfile_write(f.wfile, p, a3);
        default:             return -1;
        }
    }
    case SYS_READ: {
        if (!vmm_user_ok(a2, a3, true)) return -1;
        if (a1 >= MAX_FDS) return -1;
        OpenFile& f = t->files[a1];
        auto* p = reinterpret_cast<char*>(a2);
        switch (f.kind) {
        case FD_CONSOLE_IN: {
            if (a3 == 0) return 0;
            int c = input_getc();
            if (c == 4) return 0;                     // ctrl-D = end of input
            p[0] = static_cast<char>(c);
            return 1;
        }
        case FD_PIPE_R: return pipe_read(f.pipe, p, a3);
        case FD_FILE: {
            int64_t n = fs_read(f.index, f.pos, p, a3);
            if (n > 0) f.pos += n;
            return n;
        }
        default: return -1;
        }
    }
    case SYS_OPEN:  return sys_open(t, a1, a2);
    case SYS_CLOSE:
        if (a1 >= MAX_FDS || t->files[a1].kind == FD_NONE) return -1;
        return fd_release(t->files[a1], true) ? 0 : -1;
    case SYS_EXIT:
        fd_close_all(t, true);
        thread_exit(static_cast<int>(a1));
    case SYS_GETPID: return t->id;
    case SYS_YIELD:  yield(); return 0;
    case SYS_SLEEP:  sleep_ms(a1); return 0;
    case SYS_UPTIME: return uptime_ms();

    case SYS_SPAWN: {
        char path[64];
        if (!copy_user_str(path, a1, sizeof path)) return -1;
        auto* store = static_cast<char (*)[ARG_LEN]>(kmalloc(ARGV_MAX * ARG_LEN));
        char* av[ARGV_MAX + 1];
        int ac = store ? copy_user_argv(a2, store, av) : -1;
        int r = ac < 0 ? -1 : proc_spawn(path, ac, av);
        kfree(store);
        return r;
    }
    case SYS_FORK: {
        uint64_t root = vmm_clone_space(t->cr3);
        if (!root) return -1;
        Thread* c = thread_create_fork(t, root, frame);
        return c->id;
    }
    case SYS_EXEC: {
        char path[64];
        if (!copy_user_str(path, a1, sizeof path)) return -1;
        auto* store = static_cast<char (*)[ARG_LEN]>(kmalloc(ARGV_MAX * ARG_LEN));
        if (!store) return -1;
        char* av[ARGV_MAX + 1];
        int ac = copy_user_argv(a2, store, av);
        if (ac < 0) { kfree(store); return -1; }
        ProcImage img;
        int r = proc_prepare(path, ac, av, &img);
        kfree(store);
        if (r < 0) return -1;                       // old image is untouched on failure
        {
            IrqGuard g;
            uint64_t old = t->cr3;
            t->cr3 = img.root;
            vmm_switch(img.root);
            vmm_destroy_space(old);
        }
        t->brk_base = t->brk_cur = img.brk;
        t->mmap_next = MMAP_BASE;
        strncpy(t->name, base_name(path), sizeof t->name - 1);
        enter_usermode(img.entry, img.rsp);
        return -1;
    }
    case SYS_WAIT: {
        int status = 0;
        int r = proc_wait(static_cast<int>(a1), &status);
        if (r > 0 && a2 && vmm_user_ok(a2, 4, true)) *reinterpret_cast<int*>(a2) = status;
        return r;
    }
    case SYS_BRK: {
        if (a1 == 0) return t->brk_cur;
        if (a1 < t->brk_base || a1 > t->brk_base + BRK_MAX) return t->brk_cur;
        uint64_t oldp = (t->brk_cur + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint64_t newp = (a1 + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        for (uint64_t v = oldp; v < newp; v += PAGE_SIZE)
            if (!map_zeroed(t->cr3, v, VMM_USER | VMM_WRITE | VMM_NX)) { free_range(t->cr3, oldp, v); return t->brk_cur; }
        if (newp < oldp) free_range(t->cr3, newp, oldp);
        t->brk_cur = a1;
        return a1;
    }
    case SYS_MMAP: {
        uint64_t len = (a1 + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        if (!len || len > (256ULL << 20) || t->mmap_next + len + PAGE_SIZE >= MMAP_LIMIT) return -1;
        uint64_t flags = VMM_USER | ((a2 & 2) ? VMM_WRITE : 0) | ((a2 & 4) ? 0 : VMM_NX);
        uint64_t base = t->mmap_next;
        for (uint64_t off = 0; off < len; off += PAGE_SIZE)
            if (!map_zeroed(t->cr3, base + off, flags)) { free_range(t->cr3, base, base + off); return -1; }
        t->mmap_next += len + PAGE_SIZE;          // leave a guard page between mappings
        return base;
    }
    case SYS_MUNMAP: {
        if ((a1 & (PAGE_SIZE - 1)) || a1 < MMAP_BASE || a1 + a2 > t->mmap_next || a1 + a2 < a1) return -1;
        free_range(t->cr3, a1, (a1 + a2 + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
        return 0;
    }
    case SYS_UNLINK: {
        char path[48];
        if (!copy_user_str(path, a1, sizeof path)) return -1;
        return fs_remove(path) ? 0 : -1;
    }
    case SYS_READDIR: {
        if (!vmm_user_ok(a2, sizeof(DirInfo), true)) return -1;
        if (a1 >= (uint64_t)fs_capacity()) return -1;
        if (!fs_used(a1)) return 1;
        auto* d = reinterpret_cast<DirInfo*>(a2);
        memset(d, 0, sizeof *d);
        strncpy(d->name, fs_name(a1), sizeof d->name - 1);
        d->size = fs_size(a1);
        return 0;
    }
    case SYS_PS: {
        if (!vmm_user_ok(a1, a2 * sizeof(PsInfo), true)) return -1;
        return sched_snapshot(reinterpret_cast<PsInfo*>(a1), a2);
    }
    case SYS_MEMINFO: {
        if (!vmm_user_ok(a1, sizeof(MemInfo), true)) return -1;
        auto* m = reinterpret_cast<MemInfo*>(a1);
        m->free_frames = pmm_free_frames(); m->total_frames = pmm_total_frames();
        m->heap_kib = heap_bytes_mapped() / 1024; m->disk_free_kib = fs_free_bytes() / 1024;
        m->uptime_ms = uptime_ms(); m->threads = thread_count();
        return 0;
    }
    case SYS_DUP: {
        if (a1 >= MAX_FDS || t->files[a1].kind == FD_NONE) return -1;
        int fd = fd_alloc(t);
        if (fd < 0) return -1;
        fd_dup_into(t->files[fd], t->files[a1]);
        return fd;
    }
    case SYS_DUP2: {
        if (a1 >= MAX_FDS || a2 >= MAX_FDS || t->files[a1].kind == FD_NONE) return -1;
        if (a1 == a2) return a2;
        if (t->files[a2].kind != FD_NONE) fd_release(t->files[a2], true);
        fd_dup_into(t->files[a2], t->files[a1]);
        return a2;
    }
    case SYS_PIPE: {
        if (!vmm_user_ok(a1, 2 * sizeof(int), true)) return -1;
        int r = fd_alloc(t);
        if (r < 0) return -1;
        t->files[r].kind = FD_PIPE_R;                 // reserve so the second alloc differs
        int w = fd_alloc(t);
        if (w < 0) { t->files[r].kind = FD_NONE; return -1; }
        Pipe* p = pipe_create();
        if (!p) { t->files[r].kind = FD_NONE; return -1; }
        t->files[r].pipe = p; t->files[w].kind = FD_PIPE_W; t->files[w].pipe = p;
        auto* out = reinterpret_cast<int*>(a1);
        out[0] = r; out[1] = w;
        return 0;
    }
    default: return -38;
    }
}
