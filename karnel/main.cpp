
#include <stdint.h>
#include <stddef.h>

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

  
    for (uint64_t y = 0; y < fb->height; y++) {
        auto* row = reinterpret_cast<uint32_t*>(
            static_cast<uint8_t*>(fb->address) + y * fb->pitch);
        for (uint64_t x = 0; x < fb->width; x++) {
            uint32_t g = static_cast<uint32_t>(x * 255 / fb->width);
            uint32_t b = static_cast<uint32_t>(y * 255 / fb->height);
            row[x] = (0x20u << 16) | (g << 8) | b;
        }
    }
    halt();
}

}  // extern "C"
