#pragma once
#include <stdint.h>
#include <stddef.h>

struct OpenFile { bool used; uint32_t index; uint32_t pos; };
constexpr int MAX_FDS = 16;

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
Thread*  thread_create_user(const char* name, uint64_t cr3, uint64_t entry, uint64_t user_rsp);
[[noreturn]] void thread_exit(int code);
Thread*  thread_current();
bool     thread_alive(uint64_t id);
int      thread_exit_code(uint64_t id);                 // last recorded exit code, or -1000 if unknown
void     sched_dump();                                  // prints the thread table
size_t   thread_count();

extern "C" {
    void switch_context(uint64_t* old_rsp, uint64_t new_rsp);
    void thread_trampoline();
    void enter_usermode(uint64_t rip, uint64_t rsp);
    extern uint64_t g_syscall_kstack;
}
