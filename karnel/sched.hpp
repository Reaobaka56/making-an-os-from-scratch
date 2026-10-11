#pragma once
#include <stdint.h>
#include <stddef.h>

// Register state saved by the syscall entry stub (low to high addresses).
struct SyscallFrame {
    uint64_t r15, r14, r13, r12, rbp, rbx, r10, r9, r8, rdx, rsi, rdi, r11, rcx, rsp;
};

struct OpenFile {
    bool     used, writable;
    uint32_t index, pos;
    char     name[48];
    uint8_t* wbuf;               // write-mode files are buffered and committed on close
    uint32_t wlen, wcap;
};
constexpr int MAX_FDS = 16;

struct PsInfo { uint64_t pid, parent; char state, user; char name[24]; };

enum class TState { Ready, Running, Sleeping, Dead };

struct Thread {
    uint64_t id;
    char     name[24];
    uint64_t rsp;                // saved kernel stack pointer while not running
    uint8_t* kstack;
    uint64_t kstack_top;
    uint64_t cr3;                // address space (kernel root for kernel threads)
    TState   state;
    uint64_t wake_tick;
    bool     is_idle, is_user;
    uint64_t user_entry, user_rsp;
    uint64_t parent;
    uint64_t brk_base, brk_cur, mmap_next;
    OpenFile files[MAX_FDS];
    Thread*  next;
};

using ThreadFn = void (*)(void*);

void     sched_init();                                  // adopts the boot context as the idle thread
void     sched_tick();                                  // called from the timer IRQ: preempt
void     schedule();                                    // interrupts must be off
void     yield();
void     sleep_ms(uint64_t ms);
Thread*  thread_create(const char* name, ThreadFn fn, void* arg);
Thread*  thread_create_user(const char* name, uint64_t cr3, uint64_t entry, uint64_t user_rsp, uint64_t brk, uint64_t parent);
Thread*  thread_create_fork(Thread* parent, uint64_t cr3, const SyscallFrame* frame);
[[noreturn]] void thread_exit(int code);
Thread*  thread_current();
bool     thread_alive(uint64_t id);
int      sched_collect(uint64_t parent, int pid, int* status);   // >0 pid reaped, 0 still running, -1 no such child
size_t   sched_snapshot(PsInfo* out, size_t max);
void     close_all_files(Thread* t);                    // flush + release a process's descriptors
void     sched_dump();                                  // prints the thread table
size_t   thread_count();

extern "C" {
    void switch_context(uint64_t* old_rsp, uint64_t new_rsp);
    void thread_trampoline();
    void enter_usermode(uint64_t rip, uint64_t rsp);
    void return_to_user(SyscallFrame* frame);
    extern uint64_t g_syscall_kstack;
}
