#include "elf.hpp"
#include "vmm.hpp"
#include "pmm.hpp"
#include "mm.hpp"
#include "console.hpp"
#include <string.h>

namespace {
struct Ehdr {
    uint8_t ident[16]; uint16_t type, machine; uint32_t version;
    uint64_t entry, phoff, shoff; uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} __attribute__((packed));
struct Phdr { uint32_t type, flags; uint64_t offset, vaddr, paddr, filesz, memsz, align; } __attribute__((packed));
constexpr uint32_t PT_LOAD = 1, PF_X = 1, PF_W = 2;
}

bool elf_load(uint64_t root, const uint8_t* img, uint64_t size, uint64_t* entry, uint64_t* image_end) {
    if (size < sizeof(Ehdr)) return false;
    auto* eh = reinterpret_cast<const Ehdr*>(img);
    if (memcmp(eh->ident, "\x7f""ELF", 4) != 0 || eh->ident[4] != 2 || eh->machine != 62 || eh->type != 2) {
        kprintf("elf: not a static x86-64 executable\n");
        return false;
    }
    if (eh->phentsize != sizeof(Phdr) || eh->phoff + uint64_t(eh->phnum) * sizeof(Phdr) > size) return false;

    uint64_t top = 0;
    for (unsigned i = 0; i < eh->phnum; i++) {
        auto* ph = reinterpret_cast<const Phdr*>(img + eh->phoff + i * sizeof(Phdr));
        if (ph->type != PT_LOAD || ph->memsz == 0) continue;
        if (ph->filesz > ph->memsz || ph->offset + ph->filesz > size) return false;
        if (ph->vaddr < PAGE_SIZE || ph->vaddr + ph->memsz > 0x00007f0000000000ULL || ph->vaddr + ph->memsz < ph->vaddr) return false;

        uint64_t flags = VMM_USER | ((ph->flags & PF_W) ? VMM_WRITE : 0) | ((ph->flags & PF_X) ? 0 : VMM_NX);
        uint64_t start = ph->vaddr & ~(PAGE_SIZE - 1);
        uint64_t end = (ph->vaddr + ph->memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        if (end > top) top = end;
        for (uint64_t page = start; page < end; page += PAGE_SIZE) {
            uint64_t existing;
            uint64_t frame;
            if (vmm_translate_in(root, page, &existing)) {
                frame = existing & ~(PAGE_SIZE - 1);   // segments sharing a page
            } else {
                frame = pmm_alloc();
                if (!frame) return false;
                memset(phys_to_virt(frame), 0, PAGE_SIZE);
                if (!vmm_map_in(root, page, frame, flags)) return false;
            }
            // Copy the file-backed part of this page through the direct map.
            uint64_t seg_lo = ph->vaddr, seg_hi = ph->vaddr + ph->filesz;
            uint64_t lo = page > seg_lo ? page : seg_lo;
            uint64_t hi = page + PAGE_SIZE < seg_hi ? page + PAGE_SIZE : seg_hi;
            if (lo < hi)
                memcpy(static_cast<uint8_t*>(phys_to_virt(frame)) + (lo - page), img + ph->offset + (lo - ph->vaddr), hi - lo);
        }
    }
    *entry = eh->entry;
    *image_end = top;
    return true;
}
