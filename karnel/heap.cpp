// Kernel heap: first-fit free list, grows on demand by mapping fresh frames.
#include "heap.hpp"
#include "pmm.hpp"
#include "vmm.hpp"
#include "console.hpp"
#include "irq.hpp"

namespace {
constexpr uint64_t HEAP_BASE = 0xffffa00000000000ULL;
constexpr uint64_t HEAP_MAX  = 256ULL << 20;
constexpr uint32_t MAGIC = 0xB10CB10C;

struct alignas(16) Block { uint64_t size; Block* next; uint32_t free; uint32_t magic; };
constexpr size_t HDR = sizeof(Block);

Block* head; Block* tail;
uint64_t heap_end = HEAP_BASE;

bool grow(size_t need) {
    size_t bytes = (need + HDR + 0xFFFF) & ~size_t(0xFFFF);  // 64 KiB steps
    if (heap_end + bytes > HEAP_BASE + HEAP_MAX) return false;
    for (size_t off = 0; off < bytes; off += PAGE_SIZE) {
        uint64_t f = pmm_alloc();
        if (!f || !vmm_map(heap_end + off, f, VMM_WRITE | VMM_NX)) return false;
    }
    auto* b = reinterpret_cast<Block*>(heap_end);
    b->size = bytes - HDR; b->next = nullptr; b->free = 1; b->magic = MAGIC;
    if (tail) tail->next = b; else head = b;
    tail = b;
    heap_end += bytes;
    return true;
}

void coalesce() {
    for (Block* b = head; b && b->next;) {
        if (b->free && b->next->free) {
            if (b->next == tail) tail = b;
            b->size += HDR + b->next->size;
            b->next = b->next->next;
        } else b = b->next;
    }
}
}  // namespace

void heap_init() { grow(0); }

void* kmalloc(size_t n) {
    IrqGuard guard;
    n = n ? (n + 15) & ~size_t(15) : 16;
    for (int attempt = 0; attempt < 2; attempt++) {
        for (Block* b = head; b; b = b->next) {
            if (!b->free || b->size < n) continue;
            if (b->size >= n + HDR + 16) {  // split
                auto* r = reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(b) + HDR + n);
                r->size = b->size - n - HDR; r->next = b->next; r->free = 1; r->magic = MAGIC;
                b->size = n; b->next = r;
                if (tail == b) tail = r;
            }
            b->free = 0;
            return reinterpret_cast<uint8_t*>(b) + HDR;
        }
        if (attempt == 0 && !grow(n)) return nullptr;
        if (attempt == 0) coalesce();  // merge new block with a free tail
    }
    return nullptr;
}

void kfree(void* p) {
    if (!p) return;
    IrqGuard guard;
    auto* b = reinterpret_cast<Block*>(static_cast<uint8_t*>(p) - HDR);
    if (b->magic != MAGIC || b->free) { kprintf("heap: bad/double free %p\n", p); return; }
    b->free = 1;
    coalesce();
}

size_t heap_free_blocks() { IrqGuard guard; size_t n = 0; for (Block* b = head; b; b = b->next) if (b->free) n++; return n; }
size_t heap_bytes_mapped() { return heap_end - HEAP_BASE; }

void* operator new(size_t n) { void* p = kmalloc(n); if (!p) { kprintf("OOM\n"); for (;;) asm volatile("cli; hlt"); } return p; }
void* operator new[](size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { kfree(p); }
void operator delete[](void* p) noexcept { kfree(p); }
void operator delete(void* p, size_t) noexcept { kfree(p); }
void operator delete[](void* p, size_t) noexcept { kfree(p); }
