// Minimal kernel: asks Limine for a framebuffer and paints it.
#include <stdint.h>
#include <stddef.h>
#include "console.hpp"
#include "serial.hpp"
#include "gdt.hpp"
#include "idt.hpp"
#include "mm.hpp"

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

    // Exception test: uncomment to trigger a page fault and see the register dump.
    // *reinterpret_cast<volatile int*>(0xdeadbeef) = 1;

    kprintf("kernel idle.\n");
    halt();
}

}  // extern "C"
