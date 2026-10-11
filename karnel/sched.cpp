// Preemptive round-robin scheduler for kernel threads and user processes (single CPU).
#include "sched.hpp"
#include "apic.hpp"
#include "gdt.hpp"
#include "vmm.hpp"
#include "heap.hpp"
#include "irq.hpp"
#include "console.hpp"
#include "fs.hpp"
#include <string.h>

namespace {
constexpr size_t KSTACK = 32 * 1024;

Thread* head;
Thread* current;
Thread* idle;
uint64_t next_id = 1;

struct ExitRec { uint64_t id, parent; int code; bool collected; };
ExitRec exit_log[64];
unsigned exit_pos;

Thread* next_of(Thread* t) { return t->next ? t->next : head; }

void reap() {
    for (Thread* prev = nullptr, *t = head; t;) {
        Thread* nx = t->next;
        if (t->state == TState::Dead && t != current) {
            if (prev) prev->next = nx; else head = nx;
            if (t->is_user) vmm_destroy_space(t->cr3);
            for (auto& f : t->files) if (f.wbuf) kfree(f.wbuf);
            kfree(t->kstack);
            delete t;
        } else prev = t;
        t = nx;
    }
}

Thread* make_thread(const char* name, uint64_t cr3) {
    auto* t = new Thread();
    memset(t, 0, sizeof *t);
    t->id = next_id++;
    strncpy(t->name, name, sizeof t->name - 1);
    t->cr3 = cr3;
    t->kstack = static_cast<uint8_t*>(kmalloc(KSTACK));
    t->kstack_top = (reinterpret_cast<uint64_t>(t->kstack) + KSTACK) & ~15ULL;
    t->state = TState::Ready;
    return t;
}

void link(Thread* t) {
    IrqGuard g;
    Thread** p = &head;
    while (*p) p = &(*p)->next;
    *p = t;
}

// Lay out a fresh kernel stack so the first switch_context "returns" into thread_trampoline,
// which calls fn(arg) with interrupts enabled.
void prepare_stack(Thread* t, ThreadFn fn, void* arg, uint64_t top) {
    auto* sp = reinterpret_cast<uint64_t*>(top);
    *--sp = 0;                                         // padding (alignment)
    *--sp = 0;                                         // fake return slot
    *--sp = reinterpret_cast<uint64_t>(thread_trampoline);
    *--sp = 0;                                         // rbp
    *--sp = 0;                                         // rbx
    *--sp = reinterpret_cast<uint64_t>(fn);            // r12
    *--sp = reinterpret_cast<uint64_t>(arg);           // r13
    *--sp = 0;                                         // r14
    *--sp = 0;                                         // r15
    t->rsp = reinterpret_cast<uint64_t>(sp);
}

void fork_child_main(void* frame) { return_to_user(static_cast<SyscallFrame*>(frame)); }

void user_main(void*) {
    Thread* t = current;
    enter_usermode(t->user_entry, t->user_rsp);
}
}  // namespace

void sched_init() {
    auto* t = new Thread();
    memset(t, 0, sizeof *t);
    t->id = 0;
    strncpy(t->name, "idle", sizeof t->name - 1);
    t->cr3 = vmm_kernel_root();
    t->state = TState::Running;
    t->is_idle = true;
    head = current = idle = t;
}

Thread* thread_current() { return current; }

Thread* thread_create(const char* name, ThreadFn fn, void* arg) {
    Thread* t = make_thread(name, vmm_kernel_root());
    prepare_stack(t, fn, arg, t->kstack_top);
    link(t);
    return t;
}

Thread* thread_create_user(const char* name, uint64_t cr3, uint64_t entry, uint64_t user_rsp, uint64_t brk, uint64_t parent) {
    Thread* t = make_thread(name, cr3);
    t->is_user = true;
    t->user_entry = entry;
    t->user_rsp = user_rsp;
    t->parent = parent;
    t->brk_base = t->brk_cur = brk;
    t->mmap_next = 0x0000600000000000ULL;
    prepare_stack(t, user_main, nullptr, t->kstack_top);
    link(t);
    return t;
}

// fork: the child resumes in user mode right after the syscall, with rax = 0 and the parent's registers.
Thread* thread_create_fork(Thread* parent, uint64_t cr3, const SyscallFrame* frame) {
    Thread* t = make_thread(parent->name, cr3);
    t->is_user = true;
    t->parent = parent->id;
    t->brk_base = parent->brk_base; t->brk_cur = parent->brk_cur; t->mmap_next = parent->mmap_next;
    for (int i = 0; i < MAX_FDS; i++)                       // inherit read-only descriptors
        if (parent->files[i].used && !parent->files[i].writable) t->files[i] = parent->files[i];
    auto* fr = reinterpret_cast<SyscallFrame*>((t->kstack_top - sizeof(SyscallFrame)) & ~15ULL);
    *fr = *frame;
    prepare_stack(t, fork_child_main, fr, reinterpret_cast<uint64_t>(fr));
    link(t);
    return t;
}

void schedule() {
    uint64_t now = timer_ticks();
    for (Thread* t = head; t; t = t->next)
        if (t->state == TState::Sleeping && now >= t->wake_tick) t->state = TState::Ready;

    Thread* cur = current;
    if (cur->state == TState::Running) cur->state = TState::Ready;

    Thread* pick = nullptr;
    Thread* t = next_of(cur);
    for (;;) {
        if (t->state == TState::Ready && !t->is_idle) { pick = t; break; }
        if (t == cur) break;
        t = next_of(t);
    }
    if (!pick) pick = (idle->state == TState::Ready || idle == cur) ? idle : cur;
    if (pick->state != TState::Ready) { reap(); return; }   // nothing else runnable (cur sleeping & idle busy)

    pick->state = TState::Running;
    if (pick == cur) return;

    current = pick;
    gdt_set_rsp0(pick->kstack_top);
    g_syscall_kstack = pick->kstack_top;
    if (pick->cr3 != cur->cr3) vmm_switch(pick->cr3);
    switch_context(&cur->rsp, pick->rsp);
    // Resumed here after being switched back in.
    reap();
}

void sched_tick() { schedule(); }

void yield() { IrqGuard g; schedule(); }

void sleep_ms(uint64_t ms) {
    IrqGuard g;
    uint64_t ticks = (ms * TIMER_HZ + 999) / 1000;
    if (ticks == 0) ticks = 1;
    current->wake_tick = timer_ticks() + ticks;
    current->state = TState::Sleeping;
    schedule();
}

void thread_exit(int code) {
    IrqGuard g;
    exit_log[exit_pos++ % 64] = {current->id, current->parent, code, false};
    current->state = TState::Dead;
    for (;;) schedule();     // never resumed; reaped by the next thread
}

bool thread_alive(uint64_t id) {
    IrqGuard g;
    for (Thread* t = head; t; t = t->next) if (t->id == id && t->state != TState::Dead) return true;
    return false;
}

int sched_collect(uint64_t parent, int pid, int* status) {
    IrqGuard g;
    for (auto& r : exit_log)
        if (r.id && !r.collected && r.parent == parent && (pid < 0 || r.id == (uint64_t)pid)) {
            r.collected = true;
            if (status) *status = r.code;
            return (int)r.id;
        }
    for (Thread* t = head; t; t = t->next)
        if (t->parent == parent && t->state != TState::Dead && (pid < 0 || t->id == (uint64_t)pid)) return 0;
    return -1;
}

size_t sched_snapshot(PsInfo* out, size_t max) {
    IrqGuard g;
    static const char st[] = {'R', 'X', 'S', 'Z'};   // ready, running(X), sleeping, dead
    size_t n = 0;
    for (Thread* t = head; t && n < max; t = t->next) {
        out[n].pid = t->id; out[n].parent = t->parent;
        out[n].state = st[(int)t->state]; out[n].user = t->is_user;
        memcpy(out[n].name, t->name, sizeof out[n].name);
        n++;
    }
    return n;
}

void close_all_files(Thread* t) {
    for (auto& f : t->files) {
        if (!f.used) continue;
        if (f.writable && f.wbuf) { fs_write(f.name, f.wbuf, f.wlen); kfree(f.wbuf); f.wbuf = nullptr; }
        f.used = false;
    }
}

size_t thread_count() {
    IrqGuard g;
    size_t n = 0;
    for (Thread* t = head; t; t = t->next) if (t->state != TState::Dead) n++;
    return n;
}

void sched_dump() {
    IrqGuard g;
    static const char* st[] = {"ready", "running", "sleeping", "dead"};
    kprintf("  PID  STATE     KIND  NAME\n");
    for (Thread* t = head; t; t = t->next)
        kprintf("  %-4lu %-9s %-5s %s\n", t->id, st[(int)t->state], t->is_user ? "user" : "kern", t->name);
}

extern "C" void thread_exit_c(int code) { thread_exit(code); }
