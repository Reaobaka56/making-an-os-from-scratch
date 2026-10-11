// Kernel entry: boots Limine requests, brings up every subsystem, then becomes the idle thread.
#include <stdint.h>
#include <stddef.h>
#include "console.hpp"
#include "serial.hpp"
#include "gdt.hpp"
#include "idt.hpp"
#include "mm.hpp"
#include "apic.hpp"
#include "kbd.hpp"
#include "sched.hpp"
#include "syscall.hpp"
#include "fs.hpp"
#include "proc.hpp"
#include "pmm.hpp"
#include "shell.hpp"

namespace {

struct Framebuffer {
    void*    address;
    uint64_t width, height, pitch;
    uint16_t bpp;
};

struct FramebufferResponse {
    uint64_t      revision;
    uint64_t      count;
    Framebuffer** framebuffers;
};

struct FramebufferRequest {
    uint64_t             id[4];
    uint64_t             revision;
    FramebufferResponse* response;
};

}  // namespace

extern "C" {

// Base revision 3; Limine zeroes the last word if it supports it.
__attribute__((used, section(".limine_requests")))
volatile uint64_t base_revision[3] = {
    0xf9562b2d5c95a6c8ULL, 0x6a7b384944536bdcULL, 3};

__attribute__((used, section(".limine_requests")))
volatile FramebufferRequest fb_request = {
    {0xc7b1dd30df4c8b88ULL, 0x0a82e883a194f07bULL,
     0x9d5827dcd881dd75ULL, 0xa3148604f6fab11bULL},
    0, nullptr};

[[noreturn]] static void halt() {
    for (;;) asm volatile("cli; hlt");
}

static void worker(void* arg) {
    const char* name = static_cast<const char*>(arg);
    for (int i = 0; i < 3; i++) {
        kprintf("[%s] step %d at %lu ms\n", name, i, uptime_ms());
        sleep_ms(name[0] == 'A' ? 100 : 150);
    }
}

static void init_main(void*) {
    // Step 6 demo: two kernel threads running concurrently.
    Thread* a = thread_create("worker-A", worker, const_cast<char*>("A"));
    Thread* b = thread_create("worker-B", worker, const_cast<char*>("B"));
    while (thread_alive(a->id) || thread_alive(b->id)) sleep_ms(20);

    if (fs_mounted()) {
        // Boot demo: run /bin/hello (with arguments) in user mode and check nothing leaks.
        uint64_t free_before = pmm_free_frames();
        char* args[] = {const_cast<char*>("hello"), const_cast<char*>("from"), const_cast<char*>("the"), const_cast<char*>("kernel")};
        int pid = proc_spawn("/bin/hello", 4, args);
        if (pid > 0) {
            int status = 0;
            proc_wait(pid, &status);
            kprintf("[pid %d exited with code %d, %ld frames leaked]\n", pid, status,
                    (long)free_before - (long)pmm_free_frames());
        } else {
            kprintf("could not start /bin/hello (%d)\n", pid);
        }
        // Hand over to the user-space shell; restart it if it exits.
        while (fs_find("/bin/sh") >= 0) {
            int sh = proc_spawn("/bin/sh");
            if (sh < 0) break;
            proc_wait(sh, nullptr);
            kprintf("[shell exited, restarting]\n");
        }
    }
    kprintf("no user shell available, starting the built-in one\n");
    shell_main(nullptr);
}

void kmain() {
    if (base_revision[2] != 0) halt();  // bootloader too old
    volatile FramebufferResponse* r = fb_request.response;
    if (!r || r->count < 1) halt();

    Framebuffer* fb = r->framebuffers[0];
    if (fb->bpp != 32) halt();

    serial_init();
    console_init(fb->address, fb->width, fb->height, fb->pitch);
    kprintf("making-an-os\n");
    kprintf("framebuffer %lux%lu bpp=%u\n", fb->width, fb->height, (unsigned)fb->bpp);

    gdt_init();
    kprintf("GDT loaded\n");
    idt_init();
    kprintf("IDT loaded\n");

    mm_init();
    mm_selftest();

    apic_init();
    kbd_init();
    sched_init();
    syscall_init();
    kprintf("interrupts, scheduler and syscalls ready\n");

    if (fs_mount()) kprintf("fs: RaptorFS mounted, %u KiB free\n", fs_free_bytes() / 1024);
    else kprintf("fs: no disk or filesystem found (attach one with -drive)\n");

    thread_create("init", init_main, nullptr);
    asm volatile("sti");
    for (;;) asm volatile("hlt");       // idle thread
}

}  // extern "C"
