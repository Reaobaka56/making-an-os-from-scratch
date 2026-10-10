#include "syscall.hpp"
#include "sched.hpp"
#include "vmm.hpp"
#include "fs.hpp"
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
bool copy_user_str(char* dst, uint64_t src, size_t max) {
    for (size_t i = 0; i < max; i++) {
        if (!vmm_user_ok(src + i, 1, false)) return false;
        dst[i] = *reinterpret_cast<const char*>(src + i);
        if (!dst[i]) return true;
    }
    return false;
}
}

extern "C" int64_t syscall_dispatch(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3) {
    Thread* t = thread_current();
    switch (nr) {
    case SYS_WRITE: {
        if ((a1 != 1 && a1 != 2) || !vmm_user_ok(a2, a3, false)) return -1;
        auto* p = reinterpret_cast<const char*>(a2);
        for (uint64_t i = 0; i < a3; i++) kputc(p[i]);
        return a3;
    }
    case SYS_READ: {
        if (!vmm_user_ok(a2, a3, true)) return -1;
        auto* p = reinterpret_cast<char*>(a2);
        if (a1 == 0) { if (a3 == 0) return 0; p[0] = static_cast<char>(input_getc()); return 1; }
        if (a1 < 3 || a1 >= MAX_FDS || !t->files[a1].used) return -1;
        OpenFile& f = t->files[a1];
        int64_t n = fs_read(f.index, f.pos, p, a3);
        if (n > 0) f.pos += n;
        return n;
    }
    case SYS_OPEN: {
        char path[64];
        if (!copy_user_str(path, a1, sizeof path)) return -1;
        int idx = fs_find(path);
        if (idx < 0) return -2;
        for (int fd = 3; fd < MAX_FDS; fd++)
            if (!t->files[fd].used) { t->files[fd] = {true, (uint32_t)idx, 0}; return fd; }
        return -3;
    }
    case SYS_CLOSE:
        if (a1 < 3 || a1 >= MAX_FDS || !t->files[a1].used) return -1;
        t->files[a1].used = false;
        return 0;
    case SYS_EXIT:   thread_exit(static_cast<int>(a1));
    case SYS_GETPID: return t->id;
    case SYS_YIELD:  yield(); return 0;
    case SYS_SLEEP:  sleep_ms(a1); return 0;
    case SYS_UPTIME: return uptime_ms();
    case SYS_SPAWN: {
        char path[64];
        if (!copy_user_str(path, a1, sizeof path)) return -1;
        return proc_spawn(path);
    }
    default: return -38;
    }
}
